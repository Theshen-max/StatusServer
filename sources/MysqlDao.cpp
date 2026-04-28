//
// Created by 27044 on 26-3-11.
//
#include "../headers/MysqlDao.h"
#include "../headers/ConfigMgr.h"
#include "../headers/HashPwd.h"
// SqlConnection的实现定义
SqlConnection::SqlConnection(sql::Connection* con, int64_t lastTime) : _con(con), _lastTime(lastTime)
{

}

SqlConnection::~SqlConnection()
{
	if (_con)
		_con->close(); // 这里必须确保close()不会抛出异常
}

// MysqlPool的实现定义
MysqlPool::MysqlPool(std::string url, std::string user, std::string password,
	std::string schema, int poolSize, bool stop) :
	_url(std::move(url)),
	_user(std::move(user)),
	_password(std::move(password)),
	_schema(std::move(schema)),
	_poolSize(poolSize),
	_stop(stop)
{
	try
	{
		// 创建池内连接
		sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
		for (int i = 0; i < _poolSize; i++)
		{
			auto* connection = driver->connect(_url, _user, _password);
			connection->setSchema(_schema);
			// 获取当前时间戳
			auto currentTime = std::chrono::steady_clock::now().time_since_epoch();
			// 将时间戳转换为秒
			int64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
			_queue.push(std::make_unique<SqlConnection>(connection, timestamp));
		}

		// 创建检查线程
		_checkThread = std::jthread([this]()
		{
			while (!_stop)
			{
				// 每60s检查一次
				checkConnection();
				std::this_thread::sleep_for(std::chrono::seconds(60));
			}
		});
	}
	catch (sql::SQLException& e)
	{
		std::cerr << "Fatal Error: MySQL pool init failed! Error: " << e.what() << std::endl;
		throw std::runtime_error("Failed to initialize MySQL Connection Pool.");
	}
	catch (...)
	{
		std::cerr << "MysqlDao Unknown Error" << std::endl;
		throw std::runtime_error("Failed to initialize MySQL Connection Pool.");
	}
}

void MysqlPool::checkConnection()
{
	// 获取当前时间戳
	auto currentTime = std::chrono::steady_clock::now().time_since_epoch();
	// 时间戳转化为秒
	int64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(currentTime).count();
	{
		// 加锁处理
		std::scoped_lock<std::mutex> lock(_mutex);
		for (int i = 0; i < _queue.size(); ++i)	// 注意这里必须是当前空闲队列的实时大小，因为可能当前队列大小不是固定的
		{
			// 取出连接
			auto connection = std::move(_queue.front());
			_queue.pop();

			// 设置延期函数
			// connection是std::unique_ptr，不可拷贝，而且后期还要使用，只能引用
			Defer defer([this, &connection]
			{
				_queue.push(std::move(connection));
			});

			if (timestamp - connection->_lastTime < 5) continue;

			try
			{
				std::unique_ptr<sql::Statement> stmt(connection->_con->createStatement());
				stmt->executeQuery("select 1");
				connection->_lastTime = timestamp;
				// std::cout << "execute timer alive query, cur is " << timestamp << std::endl;
			}
			catch (sql::SQLException& e)
			{
				std::cout << "Error keeping connection alive: " << e.what() << std::endl;
				// 创建新连接替换旧连接
				sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
				auto* newConnection = driver->connect(_url, _user, _password);
				newConnection->setSchema(_schema);
				connection->_con.reset(newConnection);
				connection->_lastTime = timestamp;
			}
		}
	}
}

std::unique_ptr<SqlConnection> MysqlPool::getConnection()
{
	std::unique_lock<std::mutex> lock(_mutex);
	_cond.wait(lock, [this]
	{
		if (_stop) return true;
		return !_queue.empty();
	});
	if (_stop) return nullptr;
	std::unique_ptr<SqlConnection> connection(std::move(_queue.front()));
	_queue.pop();
	return connection;
}

void MysqlPool::returnConnection(std::unique_ptr<SqlConnection> connection)
{
	{
		std::scoped_lock<std::mutex> lock(_mutex);
		if (_stop) return;
		_queue.push(std::move(connection));
	}
	_cond.notify_one();
}

void MysqlPool::close()	// close()只代表关闭池，不代表把池的资源释放，析构函数才释放池资源
{
	{
		std::scoped_lock<std::mutex> lock(_mutex);
		_stop = true;
	}
	_cond.notify_all();
}

MysqlPool::~MysqlPool()
{
	std::scoped_lock<std::mutex> lock(_mutex);
	while (!_queue.empty()) _queue.pop();
}

// MysqlDao的实现定义
MysqlDao::MysqlDao()
{
	const std::string& host = ConfigMgr::getInstance()["Mysql"]["Host"];
	const std::string& port = ConfigMgr::getInstance()["Mysql"]["Port"];
	const std::string& user = ConfigMgr::getInstance()["Mysql"]["User"];
	const std::string& password = ConfigMgr::getInstance()["Mysql"]["Passwd"];
	const std::string& schema = ConfigMgr::getInstance()["Mysql"]["Schema"];

	_pool = std::make_unique<MysqlPool>(host + ":" + port, user, password, schema, 5);
}

MysqlDao::~MysqlDao()
{
	_pool->close(); // 必须先关闭池，让其它线程不再拿或放连接，然后成员unique_ptr析构自动调用MysqlPool析构，释放资源
}

std::optional<uint64_t> MysqlDao::registerUser(const std::string& user, const std::string& password, const std::string& email)
{
	auto connection = _pool->getConnection();
	if (!connection) return std::nullopt;
	try
	{
		Defer defer([this, &connection]
		{
			_pool->returnConnection(std::move(connection));
		});

		// 因为generateSalt生成的纯二进制数据，MySQL的VARCHAR(255)默认是UTF-8编码,所以将其转成16进制可打印字符串再传给MySQL
		std::string bndSalt = PasswordCrypto::generateSalt(password.length());
		std::string salt = PasswordCrypto::toHex(reinterpret_cast<unsigned char*>(&bndSalt[0]), bndSalt.length());
		std::string hashPwd = PasswordCrypto::hashPassword(password, salt);
		std::cout << "Username : " << user << std::endl;

		// 调用数据库的存储过程
		std::unique_ptr<sql::PreparedStatement> stmt(connection->_con->prepareStatement("call regUser(?, ?, ?, ?, @result)"));
		// 设置输入参数
		stmt->setString(1, user);
		stmt->setString(2, hashPwd);
		stmt->setString(3, email);
		stmt->setString(4, salt);
		stmt->execute();
		// 查询@result，获取结果集
		std::unique_ptr<sql::Statement> stmtResult(connection->_con->createStatement());
		std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("select @result as result"));
		// 获取结果
		if (res->next())
		{
			uint64_t result = res->getUInt64("result");
			std::cout << "Result: " << result << std::endl;	 // result保存的是UID
			return std::optional<uint64_t>(result);
		}
		// 结果集为空
		return std::nullopt;
	}
	catch (sql::SQLException& e)
	{
		std::cerr << "SQLException: " << e.what() << std::endl;
		std::cerr << "(MYSQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return std::nullopt;
	}
}

bool MysqlDao::checkEmail(const std::string& user, const std::string& email)
{
	auto connection = _pool->getConnection();
	if (!connection) return false;
	try
	{
		Defer defer([this, &connection]
		{
			_pool->returnConnection(std::move(connection));
		});

		// 准备查询语句
		std::unique_ptr<sql::PreparedStatement> pstmt(connection->_con->prepareStatement("select email from user where name = ?"));
		// 绑定参数
		pstmt->setString(1, email);
		// 执行查询
		std::unique_ptr<sql::ResultSet> result(pstmt->executeQuery());
		// 遍历结果集
		if (result->next())
		{
			const std::string checkEmail = result->getString("email");
			std::cout << "Check Email: " << checkEmail << std::endl;
			if (email != checkEmail)
			{
				std::cerr << "Email does not match check email" << std::endl;
				return false;
			}
			return true;
		}
		// 没有结果
		std::cout << "The User is not exists" << std::endl;
		return false;
	}
	catch (sql::SQLException& e)
	{
		std::cerr << "SQLException: " << e.what() << std::endl;
		std::cerr << "(MYSQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::checkPwd(const std::string& user, const std::string& password, UserInfo& userInfo)
{
	auto connection = _pool->getConnection();
	if (!connection) return false;
	try
	{
		Defer defer([this, &connection]
		{
			_pool->returnConnection(std::move(connection));
		});

		std::unique_ptr<sql::PreparedStatement> pstmt(connection->_con->prepareStatement("select * from user where name = ?"));
		pstmt->setString(1, user);
		std::unique_ptr<sql::ResultSet> result(pstmt->executeQuery());
		if (result->next())
		{
			uint64_t uid = result->getUInt64("uid");
			std::string pwd = result->getString("password");
			std::string salt = result->getString("salt");
			std::string email = result->getString("email");
			if (PasswordCrypto::hashPassword(password, salt) != pwd)
				return false;

			std::cout << "登录阶段成功获取到数据库用户信息" << std::endl;
			userInfo.uid = uid;
			userInfo.username = user;
			userInfo.email = email;
			return true;
		}
		// 结果集为空
		return false;
	}
	catch (sql::SQLException& e)
	{
		std::cerr << "SQLException: " << e.what() << std::endl;
		std::cerr << "(MYSQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

bool MysqlDao::updatePassword(const std::string& email, const std::string& password)
{
	auto connection = _pool->getConnection();
	if (!connection) return false;
	try
	{
		Defer defer([this, &connection]
		{
			_pool->returnConnection(std::move(connection));
		});

		std::string newSalt = PasswordCrypto::generateSalt(password.length());
		std::string hashPwd = PasswordCrypto::hashPassword(password, newSalt);

		// 准备查询语句
		std::unique_ptr<sql::PreparedStatement> pstmt(connection->_con->prepareStatement("update user set password = ?, salt = ? where email = ?"));
		// 绑定数据
		pstmt->setString(1, hashPwd);
		pstmt->setString(2, newSalt);
		pstmt->setString(3, email);
		// 执行并获取结果
		int res = pstmt->executeUpdate();
		std::cout << "Updated rows: " << res << std::endl;
		return true;
	}
	catch (sql::SQLException& e)
	{
		std::cerr << "SQLException: " << e.what() << std::endl;
		std::cerr << "(MYSQL error code: " << e.getErrorCode();
		std::cerr << ", SQLState: " << e.getSQLState() << " )" << std::endl;
		return false;
	}
}

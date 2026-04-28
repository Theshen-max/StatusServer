#ifndef MYSQLDAO_H
#define MYSQLDAO_H

#include "const.h"
#include <thread>
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/exception.h>
#include <jdbc/cppconn/resultset.h>

// 封装的Mysql连接
class SqlConnection
{
public:
	explicit SqlConnection(sql::Connection* con, int64_t lastTime);
	int64_t _lastTime;
	std::unique_ptr<sql::Connection> _con;	// 用std::unique_ptr让其自动回收

	~SqlConnection();
};

class MysqlPool
{
public:
	explicit MysqlPool(
		std::string url,
		std::string user,
		std::string password,
		std::string schema,
		int poolSize,
		bool stop = false
	);

	void checkConnection();

	std::unique_ptr<SqlConnection> getConnection();

	void returnConnection(std::unique_ptr<SqlConnection> connection);

	void close();

	~MysqlPool();

private:
	// 数据
	std::string _url;
	std::string _user;
	std::string _password;
	std::string _schema;
	int _poolSize;
	bool _stop;
	// 实现有锁队列
	std::queue<std::unique_ptr<SqlConnection>> _queue;
	std::mutex _mutex;
	std::condition_variable _cond;
	std::jthread _checkThread;	// 检测线程，判断连接有没有超过规定时间未通信，有则通知Mysql服务器，不要删除，心跳机制
};

/// 当前生成的uid是uuid_short()函数生成的，只支持只有一个服务器ID时的状况，推荐使用snowflake算法（待设计）
class MysqlDao
{
public:
	MysqlDao();

	~MysqlDao();

	std::optional<uint64_t> registerUser(const std::string& user, const std::string& password, const std::string& email);

	bool checkEmail(const std::string& user, const std::string& email);

	bool checkPwd(const std::string& user, const std::string& password, UserInfo& userInfo);

	bool updatePassword(const std::string& email, const std::string& password);

private:
	std::unique_ptr<MysqlPool> _pool;
};

#endif //MYSQLDAO_H

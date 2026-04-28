#include "../headers/StatusServer.h"
#include "../headers/ConfigMgr.h"
#include "../headers/RedisMgr.h"
#include "../headers/RapidJsonMgr.h"
std::string generate_unique_string() {
	// 使用 thread_local，每个线程只初始化一次生成器，大幅提升性能，同时避免数据竞争
	thread_local boost::uuids::random_generator generator;
	boost::uuids::uuid uuid = generator();
	return boost::uuids::to_string(uuid);
}

StatusServiceImpl::StatusServiceImpl(size_t poolSize) : _pool(poolSize)
{
	// ChatServer 启动后会自动向 Redis 注册自己
}

ServerUnaryReactor* StatusServiceImpl::GetChatServer(CallbackServerContext* ctx, const GetChatServerReq* req,
                                                     GetChatServerRsp* rsp)
{
	std::cout << "当前执行StatusServer的GetChatServer grpc响应" << std::endl;
	auto* reactor = ctx->DefaultReactor();
	// 每个RPC请求都有独立的ctx，线程安全，但不建议跨线程使用，所以不建议捕获到线程池
	boost::asio::post(_pool, [req, rsp, reactor, self = shared_from_this()]
	{
		try
		{
			ChatServer server;
			// 从 Redis 获取当前最空闲且存活的服务器
			if (!self->getChatServer(server))
			{
				std::cerr << "No chat servers available" << std::endl;
				rsp->set_error(ErrorCodes::RPCFailed);
				reactor->Finish(Status(grpc::StatusCode::UNAVAILABLE, "No chat servers available"));
				return;
			}
			// req, rsp, reactor的生命周期能保证到Finish结束，grpc底层保证，但也由grpc底层释放
			std::cout << "ServerAddress: " << server.host << ":" << server.port << std::endl;
			std::string prefix("StatusServer has received: ");
			rsp->set_host(server.host);
			rsp->set_port(server.port);
			// 设置基本数据
			rsp->set_error(ErrorCodes::Success);
			rsp->set_token(generate_unique_string());
			rsp->set_uid(req->uid());
			// 保存热点数据到Redis
			UserInfo info;
			info._uid = std::to_string(req->uid());
			info._token = rsp->token();
			info._serverIp = server.host;
			info._serverPort = server.port;
			info._serverName = server.name;
			self->insertUserInfo(std::to_string(req->uid()), info);
			reactor->Finish(Status::OK);
		}
		catch (const std::exception& e)
		{
			rsp->set_host("");
			rsp->set_port("0");
			rsp->set_error(ErrorCodes::RPCFailed);
			reactor->Finish(Status(grpc::StatusCode::INTERNAL, "getChatServer failed"));
		}
	});
	return reactor;
}

ServerUnaryReactor* StatusServiceImpl::Login(CallbackServerContext* ctx, const LoginReq* req, LoginRsp* rsp)
{
	std::cout << "当前执行StatusServer的Login grpc响应" << std::endl;
	auto* reactor = ctx->DefaultReactor();
	boost::asio::post(_pool, [req, rsp, reactor, self = shared_from_this()]
	{
		try
		{
			const std::string& uid = req->uid();
			auto optionalString = RedisMgr::getServerConfigRedis().get(uid);
			if (!optionalString)
			{
				rsp->set_error(ErrorCodes::UidInvalid);
				reactor->Finish(Status::OK);
				return;
			}
			const std::string& jsonStr = optionalString.value();
			rapidjson::Document doc = RapidJsonMgr::parseJson(jsonStr);
			if (doc.HasMember("parse_error"))
			{
				// JSON 格式错误处理
				rsp->set_error(ErrorCodes::RPCFailed);
				reactor->Finish(Status::OK);
				return;
			}

			std::string token = RapidJsonMgr::toString(doc, "token");

			if (token != req->token())
			{
				rsp->set_error(ErrorCodes::TokenInvalid);
				reactor->Finish(Status::OK);
				return;
			}
			rsp->set_error(ErrorCodes::Success);
			rsp->set_uid(req->uid());
			rsp->set_token(req->token());
			reactor->Finish(Status::OK);
		}
		catch (const std::exception& e)
		{
			std::cout << "StatusServer::Login error" << e.what() << std::endl;
			rsp->set_error(ErrorCodes::RPCFailed);
			reactor->Finish(Status(grpc::StatusCode::INTERNAL, "login failed"));
		}
	});
	return reactor;
}

// 核心负载均衡与故障剔除机制
bool StatusServiceImpl::getChatServer(ChatServer& outServer)
{
	// 涉及三个key-value:  key-ZSet, key-String, key-Hash
	auto& redis = RedisMgr::getServerConfigRedis();
	std::vector<std::string> servers;
	redis.zrange("chat_servers", 0, -1, std::back_inserter(servers));

	for (const auto& name: servers)
	{
		// 校验该服务器的心跳 TTL 是否还在
		if (redis.exists("ServerAlive:" + name))
		{
			std::unordered_map<std::string, std::string> serverData;
			redis.hgetall("ServerData:" + name, std::inserter(serverData, serverData.begin()));
			if (!serverData.empty())
			{
				outServer.name = name;
				outServer.host = serverData["serverIp"];
				outServer.port = serverData["serverPort"];
				return true;
			}
		}
		else
		{
			std::cerr << "[StatusServer] 发现僵尸服务器 " << name << "，触发剔除机制！" << std::endl;
			redis.zrem("chat_servers", name);
			redis.del("ServerData:" + name);
		}
	}
	// 没有有效的服务器
	return false;
}

void StatusServiceImpl::insertUserInfo(const std::string& uid, const UserInfo& userInfo)
{
	rapidjson::Document doc = RapidJsonMgr::createDocument();

	// 将 UserInfo 结构体的字段写入 JSON
	RapidJsonMgr::addMember(doc, "uid", userInfo._uid);
	RapidJsonMgr::addMember(doc, "serverName", userInfo._serverName);
	RapidJsonMgr::addMember(doc, "serverIp", userInfo._serverIp);
	RapidJsonMgr::addMember(doc, "serverPort", userInfo._serverPort);
	RapidJsonMgr::addMember(doc, "token", userInfo._token);

	std::string jsonStr = RapidJsonMgr::toJsonString(doc, false);
	RedisMgr::getServerConfigRedis().set(uid, jsonStr, std::chrono::seconds(300));
}

void StatusServiceImpl::removeUserInfo(const std::string& uid)
{
	RedisMgr::getServerConfigRedis().del(uid);
}

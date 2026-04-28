#ifndef STATUSSERVER_H
#define STATUSSERVER_H

#include "const.h"
#include <grpcpp/grpcpp.h>
#include "message.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerUnaryReactor;
using grpc::CallbackServerContext;
using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;
// 引用接口类到当前命名空间
using message::StatusService;

struct ChatServer
{
	std::string host;
	std::string port;
	std::string name;
};

// 继承并实现服务器接口
class StatusServiceImpl final: public StatusService::CallbackService, public std::enable_shared_from_this<StatusServiceImpl>
{
public:
	explicit StatusServiceImpl(size_t poolSize);

	// 初始化数据
	ServerUnaryReactor* GetChatServer(CallbackServerContext* ctx, const GetChatServerReq* req, GetChatServerRsp* rsp) override;

	// 根据上文的接口返回的数据进行连接 ---- 已经无用了，保证稳定，暂时不删
	ServerUnaryReactor* Login(CallbackServerContext* ctx, const LoginReq* req, LoginRsp* rsp) override;

private:
	struct UserInfo
	{
		std::string _uid;
		std::string _serverName;
		std::string _serverIp;
		std::string _serverPort;
		std::string _token;
	};

	boost::asio::thread_pool _pool;

	bool getChatServer(ChatServer& outServer);

	// StatusServer 是用来做负载均衡的（给刚登录的用户分配一台最闲的 ChatServer）。如果每次发消息都去问它目标在哪，它会成为性能瓶颈。
	void insertUserInfo(const std::string& uid, const UserInfo& userInfo);

	void removeUserInfo(const std::string& uid);
};

#endif //STATUSSERVER_H

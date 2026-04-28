#ifndef CONST_H
#define CONST_H

#include <map>
#include <unordered_map>
#include <functional>
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <boost/uuid.hpp>
#include "openssl/sha.h"
#include "openssl/rand.h"
#include "Singleton.h"
#include <optional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <memory>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;

using tcp = boost::asio::ip::tcp;
enum ErrorCodes
{
	Success = 0,
	Error_Json = 1001,	// Json解析错误
	RPCFailed = 1002,	// RPC请求失败
	VarifyExpired = 1003, // 验证码过期
	VarifyCodeError = 1004, // 验证码错误
	UserExited = 1005, // 用户已经存在
	PasswdError = 1006, // 密码错误
	EmailNotMatch = 1007, // 邮箱不匹配
	PasswdUpFailed = 1008, // 密码更新失败
	PasswdInvalid = 1009, // 登录密码错误
	UidInvalid = 1010, // 违法Uid
	TokenInvalid = 1011 // 违法Token
};

// 这里不能用std::string, 因为string是运行期堆内存分配的，而constexpr需要编译期确定，所以采用std::string_view
inline constexpr std::string_view CODE_PREFIX = "code_";

// Defer类 （延期作用，为分支结束提供抽象，避免每个分支结束都写一致的处理逻辑）（该类通过RAII实现）
class Defer
{
public:
	// 接收一个可调用对象
	explicit Defer(std::function<void()> func) : _func(std::move(func)) {}

	// 设置是否关闭defer对象, 默认启动
	void setEnabled(bool enabled) { _enabled = enabled; }

	~Defer() { if (_enabled) _func(); }
private:
	std::function<void()> _func;
	bool _enabled = true;
};

struct UserInfo
{
	uint64_t uid; // 用户UID
	std::string username; // 用户名字
	std::string password; // 用户密码
	std::string email; // 用户邮箱
};
#endif //CONST_H

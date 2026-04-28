#include "../headers/ConfigMgr.h"
#include "../headers/StatusServer.h"
#include "../headers/const.h"

// 读文件函数
std::string readFile(const std::string& path)
{
    // 文件输入流
    std::ifstream file(path);
    // 使用流迭代器来构造std::string, 默认构造的迭代器表示流的结束，与end()类似
    // istreambuf_iterator会读取原始缓冲区的全部字节
    // istream_iterator会根据输入格式来读取
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}
// 启动StatusServer服务
void RunServer()
{
    ConfigMgr& cfg = ConfigMgr::getInstance();
    std::string serverAddress = cfg["StatusServer"]["Host"] + ":" + cfg["StatusServer"]["Port"];

    auto service = std::make_shared<StatusServiceImpl>(4);
    // 创建Server服务构造器
    grpc::ServerBuilder builder;

    // 配置ssl
    grpc::SslServerCredentialsOptions::PemKeyCertPair keycert = {
        readFile("server.key"),
        readFile("server.crt")
    };

    grpc::SslServerCredentialsOptions sslOptions;
    sslOptions.pem_key_cert_pairs.push_back(keycert);

    // 监听端口，添加服务
    builder.AddListeningPort(serverAddress, grpc::SslServerCredentials(sslOptions));
    builder.RegisterService(service.get());

    // 构建并启动gRPC服务器
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (server == nullptr)
    {
        std::cerr << "StatusServer started failed." << std::endl;
        return;
    }
    std::cout << "StatusServer listening on " << serverAddress << std::endl;

    // 创建Asio上下文
    boost::asio::io_context ioc{1};
    auto work = boost::asio::make_work_guard(ioc);

    // 创建signal_set用于捕获信号
    boost::asio::signal_set signals{ioc, SIGINT, SIGTERM};

    // 启动异步等待
    signals.async_wait([&server, &ioc, &work](boost::system::error_code error, int signal_number)
    {
        if (error) return; // 出错
        std::cout << "Shutting down Server" << std::endl;
        server->Shutdown(); // 优雅关闭服务器
        work.reset();
        ioc.stop(); // 停止ioc, 结束run()
    });

    // 单独线程跑io_context
    std::jthread ioThread([&ioc]
    {
       ioc.run();
    });

    // 等待服务器关闭, 因为server->wait()与ioc.run()都是阻塞、事件轮询，为了避免主线程服务器wait(),单独创建线程跑ioc.run()
    server->Wait();
    // 此时Wait()结束，代表服务器关闭，需要调用ioc.stop(),保证退出run()，不然jthread析构自动调用join()，会一直等到run()结束
    work.reset();
    ioc.stop();
}

#include <sstream>
int main() {
    try
    {
        RunServer();
        /// 下面是测试当前编译器字节序，x1 == 0x22就是小端序
        // uint16_t port = 0x1122;
        // char x1, x2;
        // x1 = reinterpret_cast<char*>(&port)[0];
        // x2 = reinterpret_cast<char*>(&port)[1];
        // std::cout << std::hex << std::showbase <<  static_cast<unsigned short>(x1) << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
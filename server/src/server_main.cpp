#include "chatroom_server.h"
#include "logger.h"
#include "stream_logger.h"
#include "server_config.h"
#include <csignal>
#include <memory>

ChatRoomServer* g_server = nullptr;

void signalHandler(int signum) {
    LOG_INFO("收到信号 {}, 正在关闭服务器...", signum);
    if (g_server) {
        g_server->stop();
    }
    exit(signum);
}

int main(int argc, char* argv[]) {
    // Load configuration
    ServerConfig::instance().load("conf/server.yaml");

    // Allow port override from command line
    if (argc > 1) {
        ServerConfig::instance().port = std::atoi(argv[1]);
    }
    
    // Configure logger based on config
    const auto& logCfg = ServerConfig::instance().logging;
    Logger::instance().configure(logCfg.console_output, logCfg.file_path, logCfg.level);

    initLoggerForStdStreams();
    
    LOG_INFO("===== 聊天室服务器 =====");
    LOG_INFO("端口: {}", ServerConfig::instance().port);
    
    // 注册信号处理器
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        ChatRoomServer server(ServerConfig::instance().port);
        g_server = &server;
        server.start();
        g_server = nullptr;
    } catch (const std::exception& e) {
        LOG_ERROR("服务器异常: {}", e.what());
        return 1;
    }
    
    return 0;
}

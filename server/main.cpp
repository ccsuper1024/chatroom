#include "chatroom/chatroom_server.h"
#include "logger.h"
#include "stream_logger.h"
#include "utils/server_config.h"
#include <csignal>
#include <memory>

int main(int argc, char* argv[]) {
    // Block signals globally so they can be handled by SignalFd in ChatRoomServer
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &mask, nullptr) != 0) {
        perror("pthread_sigmask");
        return 1;
    }

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
    
    try {
        ChatRoomServer server(ServerConfig::instance().port);
        server.start();
    } catch (const std::exception& e) {
        LOG_ERROR("服务器异常: {}", e.what());
        return 1;
    }
    
    return 0;
}

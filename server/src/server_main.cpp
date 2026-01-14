#include "chatroom_server.h"
#include <spdlog/spdlog.h>
#include <csignal>
#include <memory>

std::unique_ptr<ChatRoomServer> g_server;

void signalHandler(int signum) {
    spdlog::info("收到信号 {}, 正在关闭服务器...", signum);
    if (g_server) {
        g_server->stop();
    }
    exit(signum);
}

int main(int argc, char* argv[]) {
    // 设置日志级别
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
    
    // 解析端口参数
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    spdlog::info("===== 聊天室服务器 =====");
    spdlog::info("端口: {}", port);
    
    // 注册信号处理器
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        g_server = std::make_unique<ChatRoomServer>(port);
        g_server->start();
    } catch (const std::exception& e) {
        spdlog::error("服务器异常: {}", e.what());
        return 1;
    }
    
    return 0;
}

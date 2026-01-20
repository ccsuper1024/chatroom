#include "chatroom_client.h"
#include "client_config.h"
#include "stream_logger.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>

std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\n接收到退出信号，正在退出..." << std::endl;
        g_running = false;
    }
}

void receiveMessages(ChatRoomClient& client) {
    HeartbeatConfig cfg = getHeartbeatConfig();
    while (g_running) {
        client.sendHeartbeat();
        auto messages = client.getMessages();
        for (const auto& msg : messages) {
            std::cout << "[" << msg.timestamp << "] " 
                      << msg.username << ": " 
                      << msg.content << std::endl;
        }
        // 使用更短的sleep间隔以便更快响应退出信号
        for (int i = 0; i < cfg.interval_seconds * 10 && g_running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void printHelp() {
    std::cout << "\n=== 帮助菜单 ===" << std::endl;
    std::cout << "/help   - 显示此帮助信息" << std::endl;
    std::cout << "/users  - 显示在线用户列表" << std::endl;
    std::cout << "/stats  - 显示服务器统计信息" << std::endl;
    std::cout << "/quit   - 退出聊天室" << std::endl;
    std::cout << "直接输入文本发送消息" << std::endl;
    std::cout << "================" << std::endl;
}

int main(int argc, char* argv[]) {
    // 注册信号处理
    std::signal(SIGINT, signalHandler);

    initLoggerForStdStreams();
    std::string server_host = "127.0.0.1";
    int server_port = 8080;
    
    if (argc > 1) {
        server_host = argv[1];
    }
    if (argc > 2) {
        server_port = std::atoi(argv[2]);
    }
    
    std::cout << "===== 聊天室客户端 =====" << std::endl;
    std::cout << "服务器: " << server_host << ":" << server_port << std::endl;
    std::cout << std::endl;
    
    try {
        // 创建客户端
        ChatRoomClient client(server_host, server_port);
        
        // 登录
        std::string username;
        std::cout << "请输入用户名: ";
        if (!std::getline(std::cin, username) || username.empty()) {
            return 1;
        }
        
        if (!client.login(username)) {
            std::cerr << "登录失败！" << std::endl;
            return 1;
        }
        
        std::cout << "登录成功！欢迎 " << username << std::endl;
        std::cout << "输入消息并按回车发送，输入 /help 查看命令" << std::endl;
        std::cout << "========================" << std::endl;
        std::cout << std::endl;
        
        // 启动接收消息线程
        std::thread receive_thread(receiveMessages, std::ref(client));
        
        // 主循环：发送消息
        std::string input;
        while (g_running && std::getline(std::cin, input)) {
            if (input.empty()) {
                continue;
            }
            
            if (input[0] == '/') {
                if (input == "/quit") {
                    std::cout << "正在退出..." << std::endl;
                    g_running = false;
                    break;
                } else if (input == "/help") {
                    printHelp();
                } else if (input == "/users") {
                    auto users = client.getUsers();
                    std::cout << "\n=== 在线用户 (" << users.size() << ") ===" << std::endl;
                    std::cout << std::left << std::setw(20) << "用户名" 
                              << std::setw(15) << "在线时长(s)" 
                              << std::setw(15) << "空闲时长(s)" << std::endl;
                    std::cout << std::string(50, '-') << std::endl;
                    for (const auto& u : users) {
                        std::cout << std::left << std::setw(20) << u.username 
                                  << std::setw(15) << u.online_seconds 
                                  << std::setw(15) << u.idle_seconds << std::endl;
                    }
                    std::cout << "===========================" << std::endl;
                } else if (input == "/stats") {
                    std::string stats = client.getStats();
                    std::cout << "\n=== 服务器统计 ===\n" << stats << "\n==================" << std::endl;
                } else {
                    std::cout << "未知命令，输入 /help 查看可用命令" << std::endl;
                }
            } else {
                if (!client.sendMessage(input)) {
                    std::cerr << "发送消息失败！" << std::endl;
                }
            }
        }
        
        g_running = false;
        
        // 等待接收线程结束
        if (receive_thread.joinable()) {
            receive_thread.join();
        }
    } catch (const std::exception& e) {
        std::cerr << "发生错误: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "已退出聊天室" << std::endl;
    
    return 0;
}

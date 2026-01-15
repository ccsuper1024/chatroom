#include "chatroom_client.h"
#include "client_config.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

std::atomic<bool> g_running{true};

void receiveMessages(ChatRoomClient& client) {
    HeartbeatConfig cfg = getHeartbeatConfig();
    while (g_running) {
        auto messages = client.getMessages();
        for (const auto& msg : messages) {
            std::cout << msg << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(cfg.interval_seconds));
    }
}

int main(int argc, char* argv[]) {
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
    
    // 创建客户端
    ChatRoomClient client(server_host, server_port);
    
    // 登录
    std::string username;
    std::cout << "请输入用户名: ";
    std::getline(std::cin, username);
    
    if (!client.login(username)) {
        std::cerr << "登录失败！" << std::endl;
        return 1;
    }
    
    std::cout << "登录成功！欢迎 " << username << std::endl;
    std::cout << "输入消息并按回车发送，输入 /quit 退出" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << std::endl;
    
    // 启动接收消息线程
    std::thread receive_thread(receiveMessages, std::ref(client));
    
    // 主循环：发送消息
    std::string input;
    while (g_running) {
        std::getline(std::cin, input);
        
        if (input.empty()) {
            continue;
        }
        
        if (input == "/quit") {
            std::cout << "正在退出..." << std::endl;
            g_running = false;
            break;
        }
        
        if (!client.sendMessage(input)) {
            std::cerr << "发送消息失败！" << std::endl;
        }
    }
    
    // 等待接收线程结束
    if (receive_thread.joinable()) {
        receive_thread.join();
    }
    
    std::cout << "已退出聊天室" << std::endl;
    
    return 0;
}

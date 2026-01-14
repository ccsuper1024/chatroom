#pragma once

#include <string>
#include <vector>

/**
 * 聊天室客户端
 */
class ChatRoomClient {
public:
    ChatRoomClient(const std::string& server_host, int server_port);
    
    // 登录
    bool login(const std::string& username);
    
    // 发送消息
    bool sendMessage(const std::string& content);
    
    // 获取新消息
    std::vector<std::string> getMessages();
    
    // 获取用户名
    std::string getUsername() const { return username_; }

private:
    std::string server_host_;
    int server_port_;
    std::string username_;
    size_t last_message_count_;
    
    // 发送HTTP请求
    std::string sendHttpRequest(const std::string& method, 
                               const std::string& path, 
                               const std::string& body = "");
};

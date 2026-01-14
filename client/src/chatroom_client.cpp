#include "chatroom_client.h"
#include "json_utils.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

ChatRoomClient::ChatRoomClient(const std::string& server_host, int server_port)
    : server_host_(server_host), server_port_(server_port), last_message_count_(0) {
}

bool ChatRoomClient::login(const std::string& username) {
    try {
        json request;
        request["username"] = username;
        
        std::string response = sendHttpRequest("POST", "/login", request.dump());
        
        auto resp_json = json::parse(response);
        if (resp_json["success"]) {
            username_ = username;
            spdlog::info("登录成功: {}", username_);
            return true;
        } else {
            spdlog::error("登录失败");
            return false;
        }
    } catch (const std::exception& e) {
        spdlog::error("登录异常: {}", e.what());
        return false;
    }
}

bool ChatRoomClient::sendMessage(const std::string& content) {
    try {
        json request;
        request["username"] = username_;
        request["content"] = content;
        
        std::string response = sendHttpRequest("POST", "/send", request.dump());
        
        auto resp_json = json::parse(response);
        return resp_json["success"];
    } catch (const std::exception& e) {
        spdlog::error("发送消息异常: {}", e.what());
        return false;
    }
}

std::vector<std::string> ChatRoomClient::getMessages() {
    std::vector<std::string> new_messages;
    
    try {
        std::string response = sendHttpRequest("GET", "/messages");
        
        auto resp_json = json::parse(response);
        if (resp_json["success"]) {
            auto messages = resp_json["messages"];
            
            // 只返回新消息
            for (size_t i = last_message_count_; i < messages.size(); ++i) {
                std::ostringstream oss;
                oss << "[" << messages[i]["timestamp"].get<std::string>() << "] "
                    << messages[i]["username"].get<std::string>() << ": "
                    << messages[i]["content"].get<std::string>();
                new_messages.push_back(oss.str());
            }
            
            last_message_count_ = messages.size();
        }
    } catch (const std::exception& e) {
        spdlog::error("获取消息异常: {}", e.what());
    }
    
    return new_messages;
}

std::string ChatRoomClient::sendHttpRequest(const std::string& method, 
                                           const std::string& path, 
                                           const std::string& body) {
    // 创建套接字
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        throw std::runtime_error("创建套接字失败");
    }
    
    // 连接服务器
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port_);
    
    if (inet_pton(AF_INET, server_host_.c_str(), &server_addr.sin_addr) <= 0) {
        close(sock);
        throw std::runtime_error("无效的服务器地址");
    }
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        throw std::runtime_error("连接服务器失败");
    }
    
    // 构建HTTP请求
    std::ostringstream request;
    request << method << " " << path << " HTTP/1.1\r\n";
    request << "Host: " << server_host_ << "\r\n";
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << body.size() << "\r\n";
    request << "\r\n";
    request << body;
    
    std::string request_str = request.str();
    
    // 发送请求
    send(sock, request_str.c_str(), request_str.size(), 0);
    
    // 接收响应
    char buffer[4096] = {0};
    ssize_t bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0);
    
    close(sock);
    
    if (bytes_read <= 0) {
        throw std::runtime_error("接收响应失败");
    }
    
    std::string response(buffer, bytes_read);
    
    // 提取响应体
    size_t body_start = response.find("\r\n\r\n");
    if (body_start != std::string::npos) {
        return response.substr(body_start + 4);
    }
    
    return "";
}

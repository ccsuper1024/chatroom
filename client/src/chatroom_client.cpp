#include "chatroom_client.h"
#include "json_utils.h"
#include "client_config.h"
#include "logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

ChatRoomClient::ChatRoomClient(const std::string& server_host, int server_port)
    : server_host_(server_host), server_port_(server_port), last_message_count_(0), sock_fd_(-1) {
    connectToServer();
}

ChatRoomClient::~ChatRoomClient() {
    closeConnection();
}

void ChatRoomClient::connectToServer() {
    if (sock_fd_ >= 0) {
        return;
    }

    sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd_ < 0) {
        throw std::runtime_error("创建套接字失败");
    }

    int opt = 1;
    setsockopt(sock_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

#ifdef TCP_KEEPIDLE
    int keep_idle = 30;
    setsockopt(sock_fd_, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
#endif
#ifdef TCP_KEEPINTVL
    int keep_intvl = 10;
    setsockopt(sock_fd_, IPPROTO_TCP, TCP_KEEPINTVL, &keep_intvl, sizeof(keep_intvl));
#endif
#ifdef TCP_KEEPCNT
    int keep_cnt = 3;
    setsockopt(sock_fd_, IPPROTO_TCP, TCP_KEEPCNT, &keep_cnt, sizeof(keep_cnt));
#endif
    
    // 连接服务器
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port_);
    
    if (inet_pton(AF_INET, server_host_.c_str(), &server_addr.sin_addr) <= 0) {
        closeConnection();
        throw std::runtime_error("无效的服务器地址");
    }
    
    if (connect(sock_fd_, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        closeConnection();
        throw std::runtime_error("连接服务器失败");
    }
    
    Logger::instance().info("已连接服务器 {}:{}", server_host_, server_port_);
}

void ChatRoomClient::closeConnection() {
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

bool ChatRoomClient::login(const std::string& username) {
    try {
        json request;
        request["username"] = username;
        
        std::string response = sendHttpRequest("POST", "/login", request.dump());
        
        auto resp_json = json::parse(response);
        if (resp_json["success"]) {
            username_ = username;
            Logger::instance().info("登录成功: {}", username_);
            return true;
        } else {
            Logger::instance().error("登录失败");
            return false;
        }
    } catch (const std::exception& e) {
        Logger::instance().error("登录异常: {}", e.what());
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
        Logger::instance().error("发送消息异常: {}", e.what());
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
        Logger::instance().error("获取消息异常: {}", e.what());
    }
    
    return new_messages;
}

std::string ChatRoomClient::sendHttpRequest(const std::string& method, 
                                           const std::string& path, 
                                           const std::string& body) {
    HeartbeatConfig cfg = getHeartbeatConfig();
    if (sock_fd_ < 0) {
        try {
            connectToServer();
        } catch (const std::exception& e) {
            Logger::instance().error("重新连接失败: {}", e.what());
            throw;
        }
    }

    std::ostringstream request;
    request << method << " " << path << " HTTP/1.1\r\n";
    request << "Host: " << server_host_ << "\r\n";
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << body.size() << "\r\n";
    // 保持连接
    request << "Connection: keep-alive\r\n";
    request << "\r\n";
    request << body;
    
    std::string request_str = request.str();

    auto send_with_retry = [&]() {
        for (int attempt = 0; attempt <= cfg.max_retries; ++attempt) {
            ssize_t sent = send(sock_fd_, request_str.c_str(), request_str.size(), MSG_NOSIGNAL);
            if (sent >= 0) {
                return;
            }
            Logger::instance().warn("发送失败，尝试重连..., attempt={}", attempt + 1);
            closeConnection();
            try {
                connectToServer();
            } catch (const std::exception& e) {
                Logger::instance().error("重连失败: {}", e.what());
            }
        }
        closeConnection();
        throw std::runtime_error("发送请求失败");
    };

    auto recv_with_retry = [&]() -> std::string {
        for (int attempt = 0; attempt <= cfg.max_retries; ++attempt) {
            char buffer[4096] = {0};
            ssize_t bytes_read = recv(sock_fd_, buffer, sizeof(buffer) - 1, 0);
            if (bytes_read > 0) {
                return std::string(buffer, bytes_read);
            }
            Logger::instance().warn("接收失败，尝试重连..., attempt={}", attempt + 1);
            closeConnection();
            try {
                connectToServer();
            } catch (const std::exception& e) {
                Logger::instance().error("重连失败: {}", e.what());
                continue;
            }
            ssize_t sent = send(sock_fd_, request_str.c_str(), request_str.size(), MSG_NOSIGNAL);
            if (sent < 0) {
                Logger::instance().error("重连后发送请求失败");
                continue;
            }
        }
        closeConnection();
        throw std::runtime_error("接收响应失败");
    };

    send_with_retry();
    std::string response = recv_with_retry();
    
    size_t body_start = response.find("\r\n\r\n");
    if (body_start != std::string::npos) {
        return response.substr(body_start + 4);
    }
    
    return "";
}

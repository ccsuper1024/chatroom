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
    
    // 设置接收超时
    struct timeval tv;
    tv.tv_sec = 5;  // 5秒超时
    tv.tv_usec = 0;
    setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    
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
    
    LOG_INFO("已连接服务器 {}:{}", server_host_, server_port_);
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
            if (resp_json.contains("connection_id") && resp_json["connection_id"].is_string()) {
                connection_id_ = resp_json["connection_id"].get<std::string>();
            }
            LOG_INFO("登录成功: {}, connection_id={}", username_, connection_id_);
            return true;
        } else {
            std::string error_msg = resp_json.value("error", "Unknown error");
            LOG_ERROR("登录失败: {}", error_msg);
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("登录异常: {}", e.what());
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
        LOG_ERROR("发送消息异常: {}", e.what());
        return false;
    }
}

std::vector<ClientMessage> ChatRoomClient::getMessages() {
    std::vector<ClientMessage> new_messages;
    
    try {
        std::string path = "/messages?since=" + std::to_string(last_message_count_);
        std::string response = sendHttpRequest("GET", path);
        
        auto resp_json = json::parse(response);
        if (resp_json["success"]) {
            if (resp_json.contains("messages") && resp_json["messages"].is_array()) {
                auto& messages = resp_json["messages"];
                if (!messages.empty()) {
                    for (const auto& msg : messages) {
                        ClientMessage cm;
                        cm.username = msg.value("username", "unknown");
                        cm.content = msg.value("content", "");
                        cm.timestamp = msg.value("timestamp", "");
                        new_messages.push_back(cm);
                    }
                    last_message_count_ += messages.size();
                }
            }
        } else {
            LOG_WARN("获取消息失败或格式错误: {}", response);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("获取消息异常: {}", e.what());
    }
    
    return new_messages;
}

std::vector<User> ChatRoomClient::getUsers() {
    std::vector<User> users;
    try {
        std::string response = sendHttpRequest("GET", "/users");
        auto resp_json = json::parse(response);
        
        if (resp_json.contains("users") && resp_json["users"].is_array()) {
            for (const auto& u : resp_json["users"]) {
                User user;
                user.username = u.value("username", "unknown");
                user.online_seconds = u.value("online_seconds", 0L);
                user.idle_seconds = u.value("idle_seconds", 0L);
                users.push_back(user);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("获取用户列表失败: {}", e.what());
    }
    return users;
}

std::string ChatRoomClient::getStats() {
    try {
        return sendHttpRequest("GET", "/metrics");
    } catch (const std::exception& e) {
        LOG_ERROR("获取统计信息失败: {}", e.what());
        return "{\"error\": \"获取统计信息失败\"}";
    }
}

bool ChatRoomClient::sendHeartbeat() {
    try {
        HeartbeatConfig cfg = getHeartbeatConfig();
        json request;
        request["username"] = username_;
        request["client_version"] = cfg.client_version;
        if (!connection_id_.empty()) {
            request["connection_id"] = connection_id_;
        }
        std::string response = sendHttpRequest("POST", "/heartbeat", request.dump());
        auto resp_json = json::parse(response);
        LOG_DEBUG("心跳响应: {}", response);
        return resp_json["success"];
    } catch (const std::exception& e) {
        LOG_ERROR("心跳异常: {}", e.what());
        return false;
    }
}

std::string ChatRoomClient::sendHttpRequest(const std::string& method, 
                                           const std::string& path, 
                                           const std::string& body) {
    HeartbeatConfig cfg = getHeartbeatConfig();
    if (sock_fd_ < 0) {
        try {
            connectToServer();
        } catch (const std::exception& e) {
            LOG_ERROR("重新连接失败: {}", e.what());
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
            LOG_WARN("发送失败，尝试重连..., attempt={}", attempt + 1);
            closeConnection();
            try {
                connectToServer();
            } catch (const std::exception& e) {
                LOG_ERROR("重连失败: {}", e.what());
            }
        }
        closeConnection();
        throw std::runtime_error("发送请求失败");
    };

    auto recv_with_retry = [&]() -> std::string {
        for (int attempt = 0; attempt <= cfg.max_retries; ++attempt) {
            std::string response_buffer;
            size_t header_end = std::string::npos;
            size_t content_length = 0;
            bool header_parsed = false;
            
            while (true) {
                char buffer[4096];
                ssize_t bytes_read = recv(sock_fd_, buffer, sizeof(buffer), 0);
                
                if (bytes_read < 0) {
                    if (errno == EINTR) continue;
                    LOG_WARN("接收出错或超时: {}", strerror(errno));
                    break;
                }
                if (bytes_read == 0) {
                    LOG_WARN("服务器关闭连接");
                    break;
                }
                
                response_buffer.append(buffer, bytes_read);
                
                if (!header_parsed) {
                    header_end = response_buffer.find("\r\n\r\n");
                    if (header_end != std::string::npos) {
                        std::string header = response_buffer.substr(0, header_end);
                        size_t key_pos = header.find("Content-Length:");
                        if (key_pos != std::string::npos) {
                            size_t val_start = key_pos + 15;
                            size_t val_end = header.find("\r\n", val_start);
                            if (val_end != std::string::npos) {
                                std::string val = header.substr(val_start, val_end - val_start);
                                try {
                                    content_length = std::stoull(val);
                                } catch (...) {
                                    content_length = 0;
                                }
                            }
                        }
                        header_parsed = true;
                    }
                }
                
                if (header_parsed) {
                    size_t total_needed = header_end + 4 + content_length;
                    if (response_buffer.size() >= total_needed) {
                        return response_buffer;
                    }
                }
            }

            LOG_WARN("接收失败，尝试重连..., attempt={}", attempt + 1);
            closeConnection();
            try {
                connectToServer();
            } catch (const std::exception& e) {
                LOG_ERROR("重连失败: {}", e.what());
                continue;
            }
            ssize_t sent = send(sock_fd_, request_str.c_str(), request_str.size(), MSG_NOSIGNAL);
            if (sent < 0) {
                LOG_ERROR("重连后发送请求失败");
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

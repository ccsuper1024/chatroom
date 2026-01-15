#include "chatroom_server.h"
#include "json_utils.h"
#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>

static std::atomic<unsigned long long> g_connection_counter{0};

static std::string generateConnectionId() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    unsigned long long id = ++g_connection_counter;
    std::ostringstream oss;
    oss << "conn-" << millis << "-" << id;
    return oss.str();
}

ChatRoomServer::ChatRoomServer(int port) {
    http_server_ = std::make_unique<HttpServer>(port);
    
    http_server_->registerHandler("/login", 
        [this](const HttpRequest& req) { return handleLogin(req); });
    
    http_server_->registerHandler("/send", 
        [this](const HttpRequest& req) { return handleSendMessage(req); });
    
    http_server_->registerHandler("/messages", 
        [this](const HttpRequest& req) { return handleGetMessages(req); });
    
    http_server_->registerHandler("/users", 
        [this](const HttpRequest& req) { return handleGetUsers(req); });
    
    http_server_->registerHandler("/heartbeat",
        [this](const HttpRequest& req) { return handleHeartbeat(req); });
}

void ChatRoomServer::start() {
    Logger::instance().info("聊天室服务器启动");
    http_server_->start();
}

void ChatRoomServer::stop() {
    Logger::instance().info("聊天室服务器停止");
    http_server_->stop();
}

HttpResponse ChatRoomServer::handleLogin(const HttpRequest& request) {
    HttpResponse response;
    
    try {
        auto req_json = json::parse(request.body);
        std::string username = req_json["username"];
        
        std::string connection_id = generateConnectionId();
        Logger::instance().info("用户登录: {}, connection_id={}", username, connection_id);
        
        json resp_json;
        resp_json["success"] = true;
        resp_json["message"] = "登录成功";
        resp_json["username"] = username;
        resp_json["connection_id"] = connection_id;
        
        response.body = resp_json.dump();
    } catch (const std::exception& e) {
        Logger::instance().error("处理登录请求失败: {}", e.what());
        response.status_code = 400;
        response.status_text = "Bad Request";
        
        json error_json;
        error_json["success"] = false;
        error_json["error"] = e.what();
        response.body = error_json.dump();
    }
    
    return response;
}

HttpResponse ChatRoomServer::handleSendMessage(const HttpRequest& request) {
    HttpResponse response;
    
    try {
        auto req_json = json::parse(request.body);
        
        ChatMessage msg;
        msg.username = req_json["username"];
        msg.content = req_json["content"];
        msg.timestamp = getCurrentTimestamp();
        
        {
            std::lock_guard<std::mutex> lock(messages_mutex_);
            messages_.push_back(msg);
        }
        
        Logger::instance().info("收到消息 [{}]: {}", msg.username, msg.content);
        
        json resp_json;
        resp_json["success"] = true;
        resp_json["message"] = "消息发送成功";
        
        response.body = resp_json.dump();
    } catch (const std::exception& e) {
        Logger::instance().error("处理发送消息请求失败: {}", e.what());
        response.status_code = 400;
        response.status_text = "Bad Request";
        
        json error_json;
        error_json["success"] = false;
        error_json["error"] = e.what();
        response.body = error_json.dump();
    }
    
    return response;
}

HttpResponse ChatRoomServer::handleGetMessages(const HttpRequest& request) {
    HttpResponse response;
    
    try {
        // 从查询参数获取起始位置（简化版本，这里假设从0开始）
        size_t start = 0;
        
        json resp_json;
        resp_json["success"] = true;
        resp_json["messages"] = json::array();
        
        {
            std::lock_guard<std::mutex> lock(messages_mutex_);
            for (size_t i = start; i < messages_.size(); ++i) {
                json msg_json;
                msg_json["username"] = messages_[i].username;
                msg_json["content"] = messages_[i].content;
                msg_json["timestamp"] = messages_[i].timestamp;
                resp_json["messages"].push_back(msg_json);
            }
        }
        
        response.body = resp_json.dump();
    } catch (const std::exception& e) {
        Logger::instance().error("处理获取消息请求失败: {}", e.what());
        response.status_code = 500;
        response.status_text = "Internal Server Error";
        
        json error_json;
        error_json["success"] = false;
        error_json["error"] = e.what();
        response.body = error_json.dump();
    }
    
    return response;
}

HttpResponse ChatRoomServer::handleGetUsers(const HttpRequest& request) {
    HttpResponse response;
    
    // 简化版本：返回空的在线用户列表
    // 在实际应用中，需要维护在线用户的状态
    json resp_json;
    resp_json["success"] = true;
    resp_json["users"] = json::array();
    
    response.body = resp_json.dump();
    
    return response;
}

std::string ChatRoomServer::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

HttpResponse ChatRoomServer::handleHeartbeat(const HttpRequest& request) {
    HttpResponse response;
    try {
        auto req_json = json::parse(request.body);
        std::string username = req_json.value("username", "");
        std::string version = req_json.value("client_version", "");
        std::string conn_id = req_json.value("connection_id", "");
        Logger::instance().info("收到心跳: user={}, version={}, connection_id={}", username, version, conn_id);
        json resp_json;
        resp_json["success"] = true;
        resp_json["message"] = "heartbeat ok";
        resp_json["timestamp"] = getCurrentTimestamp();
        resp_json["connection_id"] = conn_id;
        resp_json["client_version"] = version;
        response.body = resp_json.dump();
    } catch (const std::exception& e) {
        Logger::instance().error("处理心跳请求失败: {}", e.what());
        response.status_code = 400;
        response.status_text = "Bad Request";
        json error_json;
        error_json["success"] = false;
        error_json["error"] = e.what();
        response.body = error_json.dump();
    }
    return response;
}

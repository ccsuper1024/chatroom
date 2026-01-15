#include "chatroom_server.h"
#include "json_utils.h"
#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <thread>
#include <map>

static std::atomic<unsigned long long> g_connection_counter{0};

static constexpr int HEARTBEAT_TIMEOUT_SECONDS = 60;
static constexpr int SESSION_CLEANUP_INTERVAL_SECONDS = 30;

static std::string generateConnectionId() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    unsigned long long id = ++g_connection_counter;
    std::ostringstream oss;
    oss << "conn-" << millis << "-" << id;
    return oss.str();
}

ChatRoomServer::ChatRoomServer(int port)
    : running_(false) {
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
    
    http_server_->registerHandler("/metrics",
        [this](const HttpRequest& req) {
            HttpResponse resp;
            json body;
            (void)req;
            body["success"] = true;
            {
                std::lock_guard<std::mutex> lock(messages_mutex_);
                body["message_count"] = messages_.size();
            }
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                body["session_count"] = sessions_.size();
                using namespace std::chrono;
                auto now = system_clock::now();
                std::size_t active = 0;
                std::map<std::string, std::size_t> version_counts;
                for (const auto& kv : sessions_) {
                    const auto& s = kv.second;
                    auto diff = duration_cast<seconds>(now - s.last_heartbeat).count();
                    bool is_active = diff <= HEARTBEAT_TIMEOUT_SECONDS;
                    if (is_active) {
                        ++active;
                    }
                    if (!s.client_version.empty()) {
                        ++version_counts[s.client_version];
                    }
                }
                body["active_session_count"] = active;
                json versions_json;
                for (const auto& kv : version_counts) {
                    versions_json[kv.first] = kv.second;
                }
                body["client_versions"] = versions_json;
                if (start_time_.time_since_epoch().count() > 0) {
                    auto uptime = duration_cast<seconds>(now - start_time_).count();
                    body["uptime_seconds"] = uptime;
                } else {
                    body["uptime_seconds"] = 0;
                }
            }
            body["timestamp"] = getCurrentTimestamp();
            resp.body = body.dump();
            return resp;
        });
}

void ChatRoomServer::start() {
    LOG_INFO("聊天室服务器启动");
    start_time_ = std::chrono::system_clock::now();
    running_.store(true);
    cleanup_thread_ = std::thread([this]() { cleanupInactiveSessions(); });
    http_server_->start();
    running_.store(false);
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

void ChatRoomServer::stop() {
    LOG_INFO("聊天室服务器停止");
    http_server_->stop();
}

HttpResponse ChatRoomServer::handleLogin(const HttpRequest& request) {
    HttpResponse response;
    
        try {
        auto req_json = json::parse(request.body);
        std::string username = req_json["username"];
        
        std::string connection_id = generateConnectionId();
        LOG_INFO("用户登录: {}, connection_id={}", username, connection_id);
        
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            UserSession session;
            session.username = username;
            session.connection_id = connection_id;
            session.client_version.clear();
            session.last_heartbeat = std::chrono::system_clock::now();
            sessions_[connection_id] = std::move(session);
        }
        
        json resp_json;
        resp_json["success"] = true;
        resp_json["message"] = "登录成功";
        resp_json["username"] = username;
        resp_json["connection_id"] = connection_id;
        
        response.body = resp_json.dump();
    } catch (const std::exception& e) {
        LOG_ERROR("处理登录请求失败: {}", e.what());
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
        
        LOG_INFO("收到消息 [{}]: {}", msg.username, msg.content);
        
        json resp_json;
        resp_json["success"] = true;
        resp_json["message"] = "消息发送成功";
        
        response.body = resp_json.dump();
    } catch (const std::exception& e) {
        LOG_ERROR("处理发送消息请求失败: {}", e.what());
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
        (void)request;
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
        LOG_ERROR("处理获取消息请求失败: {}", e.what());
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
    
    json resp_json;
    (void)request;
    resp_json["success"] = true;
    resp_json["users"] = json::array();
    
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        using namespace std::chrono;
        auto now = system_clock::now();
        for (const auto& kv : sessions_) {
            const UserSession& s = kv.second;
            json u;
            u["username"] = s.username;
            u["connection_id"] = s.connection_id;
            u["client_version"] = s.client_version;
            auto diff = duration_cast<seconds>(now - s.last_heartbeat).count();
            bool is_active = diff <= HEARTBEAT_TIMEOUT_SECONDS;
            u["online"] = is_active;
            u["idle_seconds"] = diff;
            u["last_heartbeat"] = formatTimestamp(s.last_heartbeat);
            resp_json["users"].push_back(u);
        }
    }
    
    response.body = resp_json.dump();
    
    return response;
}

std::string ChatRoomServer::getCurrentTimestamp() {
    return formatTimestamp(std::chrono::system_clock::now());
}

std::string ChatRoomServer::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
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
            LOG_INFO("收到心跳: user={}, version={}, connection_id={}", username, version, conn_id);
        
        if (!conn_id.empty()) {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            auto it = sessions_.find(conn_id);
            if (it != sessions_.end()) {
                it->second.username = username;
                it->second.client_version = version;
                it->second.last_heartbeat = std::chrono::system_clock::now();
            }
        }
        json resp_json;
        resp_json["success"] = true;
        resp_json["message"] = "heartbeat ok";
        resp_json["timestamp"] = getCurrentTimestamp();
        resp_json["connection_id"] = conn_id;
        resp_json["client_version"] = version;
        response.body = resp_json.dump();
        } catch (const std::exception& e) {
            LOG_ERROR("处理心跳请求失败: {}", e.what());
        response.status_code = 400;
        response.status_text = "Bad Request";
        json error_json;
        error_json["success"] = false;
        error_json["error"] = e.what();
        response.body = error_json.dump();
    }
    return response;
}

void ChatRoomServer::cleanupInactiveSessions() {
    using namespace std::chrono;
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(SESSION_CLEANUP_INTERVAL_SECONDS));
        auto now = system_clock::now();
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            auto diff = duration_cast<seconds>(now - it->second.last_heartbeat).count();
            if (diff > HEARTBEAT_TIMEOUT_SECONDS) {
                LOG_INFO("移除超时会话: {} {}", it->second.username, it->second.connection_id);
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

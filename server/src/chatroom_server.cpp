#include "chatroom_server.h"
#include "json_utils.h"
#include "logger.h"
#include "server_config.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <thread>
#include <map>
#include <fstream>
#include <filesystem>

static std::atomic<unsigned long long> g_connection_counter{0};

bool ChatRoomServer::checkRateLimit(const std::string& ip) {
    const auto& config = ServerConfig::instance().rate_limit;
    if (!config.enabled) {
        return true;
    }
    
    if (ip.empty()) {
        LOG_WARN("Rate limit check skipped for empty IP");
        return true; 
    }
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto& entry = rate_limits_[ip];
    
    if (entry.reset_time < now) {
        entry.count = 0;
        entry.reset_time = now + std::chrono::seconds(config.window_seconds);
    }
    
    // LOG_INFO("Rate limit check: IP={}, count={}", ip, entry.count);

    if (entry.count >= config.max_requests) {
        LOG_WARN("IP {} rate limited", ip);
        return false;
    }
    
    entry.count++;
    return true;
}

bool ChatRoomServer::validateUsername(const std::string& username) {
    if (username.empty() || username.length() > ServerConfig::instance().max_username_length) return false;
    for (char c : username) {
        if (!isalnum(c) && c != '_') return false;
    }
    return true;
}

bool ChatRoomServer::validateMessage(const std::string& content) {
    if (content.empty() || content.length() > ServerConfig::instance().max_message_length) return false;
    for (char c : content) {
        if (iscntrl(c) && c != '\n' && c != '\t') return false;
    }
    return true;
}

static std::size_t parseSinceParam(const std::string& path) {
    auto pos = path.find('?');
    if (pos == std::string::npos) {
        return 0;
    }
    std::size_t result = 0;
    std::string query = path.substr(pos + 1);
    std::size_t start = 0;
    while (start < query.size()) {
        auto amp = query.find('&', start);
        std::string pair = query.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        auto eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = pair.substr(0, eq);
            std::string value = pair.substr(eq + 1);
            if (key == "since") {
                try {
                    result = static_cast<std::size_t>(std::stoull(value));
                } catch (...) {
                }
                break;
            }
        }
        if (amp == std::string::npos) {
            break;
        }
        start = amp + 1;
    }
    return result;
}

static std::string generateConnectionId() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    unsigned long long id = ++g_connection_counter;
    std::ostringstream oss;
    oss << "conn-" << millis << "-" << id;
    return oss.str();
}

ChatRoomServer::ChatRoomServer(int port)
    : metrics_collector_(std::make_shared<MetricsCollector>()),
      base_message_index_(0),
      running_(false) {
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
        [this](const HttpRequest& req) { return handleMetrics(req); });
}

void ChatRoomServer::start() {
    LOG_INFO("聊天室服务器启动");
    loadMessages();
    start_time_ = std::chrono::system_clock::now();
    running_.store(true);
    cleanup_thread_ = std::thread([this]() { cleanupInactiveSessions(); });
    http_server_->start();
    running_.store(false);
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

HttpResponse ChatRoomServer::handleLogin(const HttpRequest& request) {
    metrics_collector_->recordRequest("POST", "/login");

    if (!checkRateLimit(request.remote_ip)) {
        return CreateErrorResponse(ErrorCode::RATE_LIMITED);
    }
    
    try {
        auto req_json = json::parse(request.body);
        std::string username = req_json.value("username", "");
        if (!validateUsername(username)) {
            return CreateErrorResponse(ErrorCode::INVALID_USERNAME);
        }
        
        std::string connection_id = generateConnectionId();
        
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            // Check if username is already taken
            for (const auto& kv : sessions_) {
                if (kv.second.username == username) {
                    return CreateErrorResponse(ErrorCode::USERNAME_TAKEN);
                }
            }

            UserSession session;
            session.username = username;
            session.connection_id = connection_id;
            session.last_heartbeat = std::chrono::system_clock::now();
            session.login_time = session.last_heartbeat;
            sessions_[connection_id] = session;
            metrics_collector_->updateActiveSessions(sessions_.size());
        }
        
        LOG_INFO("用户登录: {} (conn_id={})", username, connection_id);
        
        json resp_json;
        resp_json["success"] = true;
        resp_json["connection_id"] = connection_id;
        
        HttpResponse response;
        response.body = resp_json.dump();
        return response;
    } catch (const std::exception& e) {
        LOG_ERROR("处理登录请求失败: {}", e.what());
        metrics_collector_->recordError("login_error");
        return CreateErrorResponse(ErrorCode::INVALID_REQUEST);
    }
}

HttpResponse ChatRoomServer::handleGetUsers(const HttpRequest& request) {
    metrics_collector_->recordRequest("GET", "/users");
    
    if (!checkRateLimit(request.remote_ip)) {
        return CreateErrorResponse(ErrorCode::RATE_LIMITED);
    }

    try {
        json resp_json;
        resp_json["success"] = true;
        resp_json["users"] = json::array();
        
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            auto now = std::chrono::system_clock::now();
            for (const auto& pair : sessions_) {
                json user;
                user["username"] = pair.second.username;
                // user["connection_id"] = pair.second.connection_id; // Optional
                
                auto idle_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - pair.second.last_heartbeat).count();
                auto online_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - pair.second.login_time).count();
                
                user["idle_seconds"] = idle_ms / 1000;
                user["online_seconds"] = online_ms / 1000;
                
                resp_json["users"].push_back(user);
            }
        }
        
        HttpResponse response;
        response.body = resp_json.dump();
        return response;
    } catch (const std::exception& e) {
        LOG_ERROR("处理获取用户列表失败: {}", e.what());
        metrics_collector_->recordError("get_users_error");
        return CreateErrorResponse(ErrorCode::INTERNAL_ERROR);
    }
}

HttpResponse ChatRoomServer::handleSendMessage(const HttpRequest& request) {
    metrics_collector_->recordRequest("POST", "/send");

    if (!checkRateLimit(request.remote_ip)) {
        return CreateErrorResponse(ErrorCode::RATE_LIMITED);
    }
    
    try {
        auto req_json = json::parse(request.body);
        std::string username = req_json.value("username", "");
        std::string content = req_json.value("content", "");
        std::string connection_id = req_json.value("connection_id", "");
        if (!connection_id.empty()) {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            auto it = sessions_.find(connection_id);
            if (it != sessions_.end()) {
                username = it->second.username;
            }
        }
        if (!validateUsername(username)) {
            return CreateErrorResponse(ErrorCode::INVALID_USERNAME);
        }
        if (!validateMessage(content)) {
            return CreateErrorResponse(ErrorCode::INVALID_MESSAGE);
        }
        ChatMessage msg;
        msg.username = username;
        msg.content = content;
        msg.timestamp = getCurrentTimestamp();
        {
            std::lock_guard<std::mutex> lock(messages_mutex_);
            messages_.push_back(msg);
            metrics_collector_->updateMessageCount(messages_.size());
            LOG_INFO("Message stored. Total messages: {}, base_index: {}", messages_.size(), base_message_index_);
            if (messages_.size() > ServerConfig::instance().max_message_history) {
                messages_.pop_front();
                ++base_message_index_;
            }
        }
        LOG_INFO("收到消息 [{}]: {}", msg.username, msg.content);
        json resp_json;
        resp_json["success"] = true;
        resp_json["message"] = "消息发送成功";
        
        HttpResponse response;
        response.body = resp_json.dump();
        return response;
    } catch (const std::exception& e) {
        LOG_ERROR("处理发送消息请求失败: {}", e.what());
        metrics_collector_->recordError("send_message_error");
        return CreateErrorResponse(ErrorCode::INVALID_REQUEST);
    }
}

HttpResponse ChatRoomServer::handleGetMessages(const HttpRequest& request) {
    metrics_collector_->recordRequest("GET", "/messages");

    if (!checkRateLimit(request.remote_ip)) {
        return CreateErrorResponse(ErrorCode::RATE_LIMITED);
    }
    
    try {
        size_t since_idx = parseSinceParam(request.path);
        
        json resp_json;
        resp_json["success"] = true;
        resp_json["messages"] = json::array();
        
        {
            std::lock_guard<std::mutex> lock(messages_mutex_);
            LOG_INFO("Handling GetMessages. since={}, base_index={}, total_msgs={}", since_idx, base_message_index_, messages_.size());
            // 客户端请求 since_idx 之后的消息
            // 我们当前的消息范围是 [base_message_index_, base_message_index_ + messages_.size())
            
            // 计算在 messages_ deque 中的起始下标
            size_t start_in_deque = 0;
            if (since_idx >= base_message_index_) {
                start_in_deque = since_idx - base_message_index_;
            } else {
                // 如果请求的 since_idx 小于 base_message_index_，
                // 说明客户端落后太多（消息已被淘汰），或者客户端是新来的（since=0）
                // 我们从头开始返回
                start_in_deque = 0;
            }

            for (size_t i = start_in_deque; i < messages_.size(); ++i) {
                json msg_json;
                msg_json["username"] = messages_[i].username;
                msg_json["content"] = messages_[i].content;
                msg_json["timestamp"] = messages_[i].timestamp;
                resp_json["messages"].push_back(msg_json);
            }
            // 返回下次请求应该使用的 since (当前最大 index + 1)
            resp_json["next_since"] = base_message_index_ + messages_.size();
        }
        
        HttpResponse response;
        response.body = resp_json.dump();
        return response;
    } catch (const std::exception& e) {
        LOG_ERROR("处理获取消息请求失败: {}", e.what());
        metrics_collector_->recordError("get_messages_error");
        return CreateErrorResponse(ErrorCode::INTERNAL_ERROR);
    }
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
    metrics_collector_->recordRequest("POST", "/heartbeat");

    if (!checkRateLimit(request.remote_ip)) {
        return CreateErrorResponse(ErrorCode::RATE_LIMITED);
    }

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
                // Only update metadata, username is immutable
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
        
        HttpResponse response;
        response.body = resp_json.dump();
        return response;
    } catch (const std::exception& e) {
        LOG_ERROR("处理心跳请求失败: {}", e.what());
        metrics_collector_->recordError("heartbeat_error");
        return CreateErrorResponse(ErrorCode::INVALID_REQUEST);
    }
}

void ChatRoomServer::cleanupInactiveSessions() {
    using namespace std::chrono;
    int interval = ServerConfig::instance().session_cleanup_interval_seconds;
    int timeout = ServerConfig::instance().heartbeat_timeout_seconds;
    
    while (running_.load()) {
        {
            std::unique_lock<std::mutex> lock(cleanup_mutex_);
            cleanup_cv_.wait_for(lock, std::chrono::seconds(interval), 
                [this]{ return !running_.load(); });
        }
        if (!running_.load()) break;

        auto now = system_clock::now();
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            auto diff = duration_cast<seconds>(now - it->second.last_heartbeat).count();
            if (diff > timeout) {
                LOG_INFO("移除超时会话: {} {}", it->second.username, it->second.connection_id);
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

HttpResponse ChatRoomServer::handleMetrics(const HttpRequest& request) {
    if (!checkRateLimit(request.remote_ip)) {
        return CreateErrorResponse(ErrorCode::RATE_LIMITED);
    }

    try {
        json body = metrics_collector_->getMetrics();
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
            int timeout = ServerConfig::instance().heartbeat_timeout_seconds;
            for (const auto& kv : sessions_) {
                const auto& s = kv.second;
                auto diff = duration_cast<seconds>(now - s.last_heartbeat).count();
                bool is_active = diff <= timeout;
                if (is_active) {
                    ++active;
                }
                if (!s.client_version.empty()) {
                    ++version_counts[s.client_version];
                }
            }
            body["active_session_count"] = active;
            metrics_collector_->updateActiveSessions(active);
            
            json versions_json;
            for (const auto& kv : version_counts) {
                versions_json[kv.first] = kv.second;
            }
            body["client_versions"] = versions_json;
        }

        // Thread pool metrics
        body["thread_pool_queue_size"] = http_server_->getThreadPoolQueueSize();
        body["thread_pool_rejected_count"] = http_server_->getThreadPoolRejectedCount();
        body["thread_pool_thread_count"] = http_server_->getThreadPoolThreadCount();
        body["thread_pool_active_thread_count"] = http_server_->getThreadPoolActiveThreadCount();

        HttpResponse resp;
        resp.body = body.dump();
        return resp;
    } catch (const std::exception& e) {
        LOG_ERROR("处理 Metrics 请求失败: {}", e.what());
        return CreateErrorResponse(ErrorCode::INTERNAL_ERROR);
    }
}

void ChatRoomServer::stop() {
    LOG_INFO("聊天室服务器停止");
    running_ = false;
    cleanup_cv_.notify_all();
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    saveMessages();
    http_server_->stop();
}

void ChatRoomServer::loadMessages() {
    std::string file_path = ServerConfig::instance().history_file_path;
    if (!std::filesystem::exists(file_path)) {
        LOG_INFO("消息历史文件不存在，跳过加载: {}", file_path);
        return;
    }

    try {
        std::ifstream f(file_path);
        json j;
        f >> j;

        if (j.contains("base_index")) {
            base_message_index_ = j["base_index"];
        }
        if (j.contains("messages") && j["messages"].is_array()) {
            std::lock_guard<std::mutex> lock(messages_mutex_);
            messages_.clear();
            for (const auto& m : j["messages"]) {
                ChatMessage msg;
                msg.username = m.value("username", "unknown");
                msg.content = m.value("content", "");
                msg.timestamp = m.value("timestamp", "");
                messages_.push_back(msg);
            }
            LOG_INFO("已加载 {} 条历史消息", messages_.size());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("加载消息历史失败: {}", e.what());
    }
}

void ChatRoomServer::saveMessages() {
    std::string file_path = ServerConfig::instance().history_file_path;
    
    // Ensure directory exists
    std::filesystem::path p(file_path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    try {
        json j;
        {
            std::lock_guard<std::mutex> lock(messages_mutex_);
            j["base_index"] = base_message_index_;
            j["messages"] = json::array();
            for (const auto& msg : messages_) {
                json m;
                m["username"] = msg.username;
                m["content"] = msg.content;
                m["timestamp"] = msg.timestamp;
                j["messages"].push_back(m);
            }
        }
        
        std::ofstream f(file_path);
        f << j.dump(4);
        LOG_INFO("已保存消息历史至 {}", file_path);
    } catch (const std::exception& e) {
        LOG_ERROR("保存消息历史失败: {}", e.what());
    }
}
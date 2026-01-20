#include "chatroom_server.h"
#include "json_utils.h"
#include "logger.h"
#include "server_config.h"
#include "database_manager.h"
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

static std::string getQueryParam(const std::string& path, const std::string& key) {
    auto pos = path.find('?');
    if (pos == std::string::npos) {
        return "";
    }
    std::string query = path.substr(pos + 1);
    std::size_t start = 0;
    while (start < query.size()) {
        auto amp = query.find('&', start);
        std::string pair = query.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
        auto eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string current_key = pair.substr(0, eq);
            if (current_key == key) {
                return pair.substr(eq + 1);
            }
        }
        if (amp == std::string::npos) {
            break;
        }
        start = amp + 1;
    }
    return "";
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
    
    // Init Database
    // Legacy logic for .json -> .db migration can be kept but we should look at config now.
    // If config type is sqlite, we do the migration logic.
    DatabaseConfig& db_config = ServerConfig::instance().db;
    
    if (db_config.type == "sqlite") {
        std::string db_path = db_config.path;
        // If path ends with .json, change it to .db to avoid conflict with legacy file
        if (db_path.size() > 5 && db_path.substr(db_path.size() - 5) == ".json") {
            db_path = db_path.substr(0, db_path.size() - 5) + ".db";
        }
        if (db_path.empty()) db_path = "chatroom.db";
        db_config.path = db_path; // update back

        // Ensure directory exists
        std::filesystem::path p(db_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    }
    
    if (!DatabaseManager::instance().init(db_config)) {
        LOG_ERROR("Failed to initialize database");
        return;
    }

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
        resp_json["username"] = username;
        
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
        std::string target_user = req_json.value("target_user", "");
        std::string room_id = req_json.value("room_id", "");
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
        msg.target_user = target_user;
        msg.room_id = room_id;
        
        if (DatabaseManager::instance().addMessage(msg)) {
             metrics_collector_->updateMessageCount(DatabaseManager::instance().getMessageCount());
             LOG_INFO("Message stored. Total messages: {}", DatabaseManager::instance().getMessageCount());
        } else {
             LOG_ERROR("Failed to store message to database");
             return CreateErrorResponse(ErrorCode::INTERNAL_ERROR);
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
        std::string since_val = getQueryParam(request.path, "since");
        long long last_id = 0;
        if (!since_val.empty()) {
            try {
                last_id = std::stoll(since_val);
            } catch (...) {}
        }

        std::string username = getQueryParam(request.path, "username");
        
        json resp_json;
        resp_json["success"] = true;
        resp_json["messages"] = json::array();
        
        auto history = DatabaseManager::instance().getMessagesAfter(last_id, username);
        
        long long max_id = last_id;
        for (const auto& msg : history) {
            json msg_json;
            msg_json["username"] = msg.username;
            msg_json["content"] = msg.content;
            msg_json["timestamp"] = msg.timestamp;
            if (!msg.target_user.empty()) msg_json["target_user"] = msg.target_user;
            if (!msg.room_id.empty()) msg_json["room_id"] = msg.room_id;
            
            resp_json["messages"].push_back(msg_json);
            if (msg.id > max_id) max_id = msg.id;
        }
        
        resp_json["next_since"] = max_id;
        
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
        // Collect latest stats before generating report
        metrics_collector_->updateMessageCount(DatabaseManager::instance().getMessageCount());

        std::size_t active_count = 0;
        std::map<std::string, std::size_t> version_counts;
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            using namespace std::chrono;
            auto now = system_clock::now();
            int timeout = ServerConfig::instance().heartbeat_timeout_seconds;
            for (const auto& kv : sessions_) {
                const auto& s = kv.second;
                auto diff = duration_cast<seconds>(now - s.last_heartbeat).count();
                if (diff <= timeout) {
                    ++active_count;
                }
                if (!s.client_version.empty()) {
                    ++version_counts[s.client_version];
                }
            }
            metrics_collector_->updateActiveSessions(active_count);
        }

        std::string prom_metrics = metrics_collector_->getPrometheusMetrics();
        std::ostringstream ss;
        ss << prom_metrics;

        // Append thread pool metrics
        ss << "# HELP chatroom_thread_pool_queue_size Current tasks in queue\n";
        ss << "# TYPE chatroom_thread_pool_queue_size gauge\n";
        ss << "chatroom_thread_pool_queue_size " << http_server_->getThreadPoolQueueSize() << "\n";

        ss << "# HELP chatroom_thread_pool_rejected_total Total rejected tasks\n";
        ss << "# TYPE chatroom_thread_pool_rejected_total counter\n";
        ss << "chatroom_thread_pool_rejected_total " << http_server_->getThreadPoolRejectedCount() << "\n";

        ss << "# HELP chatroom_thread_pool_threads Total threads\n";
        ss << "# TYPE chatroom_thread_pool_threads gauge\n";
        ss << "chatroom_thread_pool_threads " << http_server_->getThreadPoolThreadCount() << "\n";

        ss << "# HELP chatroom_thread_pool_active_threads Active threads\n";
        ss << "# TYPE chatroom_thread_pool_active_threads gauge\n";
        ss << "chatroom_thread_pool_active_threads " << http_server_->getThreadPoolActiveThreadCount() << "\n";

        // Client versions
        ss << "# HELP chatroom_client_versions Active client versions\n";
        ss << "# TYPE chatroom_client_versions gauge\n";
        for (const auto& kv : version_counts) {
             ss << "chatroom_client_versions{version=\"" << kv.first << "\"} " << kv.second << "\n";
        }

        HttpResponse resp;
        resp.body = ss.str();
        resp.content_type = "text/plain; version=0.0.4";
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
    http_server_->stop();
}
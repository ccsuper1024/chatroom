#include "chatroom/chatroom_server.h"
#include "chat_service.h"
#include "utils/json_utils.h"
#include "logger.h"
#include "utils/server_config.h"
#include "database_manager.h"
#include "net/tcp_connection.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <map>
#include <filesystem>

static std::atomic<unsigned long long> g_connection_counter{0};

bool ChatRoomServer::checkRateLimit(const std::string& ip) {
    return rate_limiter_.check(ip);
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


ChatRoomServer::ChatRoomServer(int port)
    : metrics_collector_(std::make_shared<MetricsCollector>()),
      session_manager_(std::make_unique<SessionManager>(&loop_, metrics_collector_)),
      running_(false) {
    chat_service_ = std::make_unique<ChatService>(metrics_collector_, session_manager_.get());
    http_server_ = std::make_unique<HttpServer>(&loop_, port);
    rtsp_server_ = std::make_unique<RtspServer>(&loop_, port + 1);
    sip_server_ = std::make_unique<SipServer>(&loop_, port + 2);
    ftp_server_ = std::make_unique<FtpServer>(&loop_, port + 3);
    
    http_server_->setWebSocketHandler([this](std::shared_ptr<TcpConnection> conn, const protocols::WebSocketFrame& frame) {
        handleWebSocketMessage(conn, frame);
    });
    
    http_server_->setStaticResourceDir(ServerConfig::instance().static_resource_dir);

    rtsp_server_->setRtspHandler([this](std::shared_ptr<TcpConnection> conn, const protocols::RtspRequest& req) {
        handleRtspMessage(conn, req);
    });

    sip_server_->setSipHandler([this](std::shared_ptr<TcpConnection> conn, const SipRequest& req, const std::string& raw_msg) {
        chat_service_->handleSipMessage(conn, req, raw_msg);
    });

    ftp_server_->setFtpHandler([this](std::shared_ptr<TcpConnection> conn, const std::string& command) {
        chat_service_->handleFtpMessage(conn, command);
    });

    http_server_->registerHandler("/login", 
        [this](const HttpRequest& req) { return handleLogin(req); });

    http_server_->registerHandler("/register", 
        [this](const HttpRequest& req) { return handleRegister(req); });
    
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

ChatRoomServer::~ChatRoomServer() = default;

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
    session_manager_->start();
    
    LOG_INFO("ChatRoomServer starting on ports: HTTP={}, RTSP={}, SIP={}, FTP={}", 
             http_server_->port(), rtsp_server_->port(), sip_server_->port(), ftp_server_->port());

    // Set static resource directory for frontend
    http_server_->setStaticResourceDir(ServerConfig::instance().static_resource_dir);

    http_server_->start();
    rtsp_server_->start();
    sip_server_->start();
    ftp_server_->start();
    
    loop_.loop();
    
    running_.store(false);
    session_manager_->stop();
}

void ChatRoomServer::stop() {
    loop_.stop();
    if (http_server_) {
        http_server_->stop();
    }
}

HttpResponse ChatRoomServer::handleLogin(const HttpRequest& request) {
    if (request.method == "GET") {
        return http_server_->serveStaticFile("/index.html");
    }

    metrics_collector_->recordRequest(request.method, "/login");

    if (!checkRateLimit(request.remote_ip)) {
        return CreateErrorResponse(ErrorCode::RATE_LIMITED);
    }
    
    try {
        auto req_json = json::parse(request.body);
        std::string username = req_json.value("username", "");
        std::string password = req_json.value("password", "");
        
        if (!validateUsername(username)) {
            return CreateErrorResponse(ErrorCode::INVALID_USERNAME);
        }

        // Validate password
        if (!DatabaseManager::instance().validateUser(username, password)) {
            // To prevent username enumeration, we could use a generic error, but for now specific is fine or use INVALID_REQUEST
            // Or maybe we don't have INVALID_CREDENTIALS in ErrorCode.
            // Let's assume we can return a custom error or just use INVALID_REQUEST with message
            // Checking ErrorCode enum might be needed, but CreateErrorResponse takes ErrorCode.
            // I'll stick to INVALID_REQUEST or similar if I can't find a better one.
            // Actually, let's look at CreateErrorResponse implementation later if needed.
            // For now, I'll log and return error.
            LOG_WARN("Login failed for user {}: invalid credentials", username);
            // Construct manual error response since I don't know if INVALID_CREDENTIALS exists
            json error_json;
            error_json["success"] = false;
            error_json["error"] = "Invalid username or password";
            HttpResponse response;
            response.body = error_json.dump();
            return response;
        }
        
        auto result = session_manager_->login(username);
        if (!result.success) {
            return CreateErrorResponse(ErrorCode::USERNAME_TAKEN);
        }
        
        LOG_INFO("用户登录: {} (conn_id={}, user_id={})", username, result.connection_id, result.user_id);
        
        json resp_json;
        resp_json["success"] = true;
        resp_json["connection_id"] = result.connection_id;
        resp_json["user_id"] = result.user_id;
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

HttpResponse ChatRoomServer::handleRegister(const HttpRequest& request) {
    metrics_collector_->recordRequest("POST", "/register");

    if (!checkRateLimit(request.remote_ip)) {
        return CreateErrorResponse(ErrorCode::RATE_LIMITED);
    }

    try {
        auto req_json = json::parse(request.body);
        std::string username = req_json.value("username", "");
        std::string password = req_json.value("password", "");

        if (!validateUsername(username)) {
            return CreateErrorResponse(ErrorCode::INVALID_USERNAME);
        }

        if (password.empty()) {
            json error_json;
            error_json["success"] = false;
            error_json["error"] = "Password cannot be empty";
            HttpResponse response;
            response.body = error_json.dump();
            return response;
        }

        if (DatabaseManager::instance().userExists(username)) {
            return CreateErrorResponse(ErrorCode::USERNAME_TAKEN);
        }

        if (DatabaseManager::instance().addUser(username, password)) {
            LOG_INFO("User registered: {}", username);
            json resp_json;
            resp_json["success"] = true;
            resp_json["username"] = username;
            HttpResponse response;
            response.body = resp_json.dump();
            return response;
        } else {
            LOG_ERROR("Failed to register user: {}", username);
            return CreateErrorResponse(ErrorCode::INTERNAL_ERROR);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Register failed: {}", e.what());
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
        
        auto sessions = session_manager_->getAllSessions();
        auto now = std::chrono::system_clock::now();
        
        for (const auto& session : sessions) {
            json user;
            user["username"] = session.username;
            user["user_id"] = session.user_id;
            user["client_type"] = session.client_type;
            
            auto idle_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - session.last_heartbeat).count();
            auto online_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - session.login_time).count();
            
            user["idle_seconds"] = idle_ms / 1000;
            user["online_seconds"] = online_ms / 1000;
            
            resp_json["users"].push_back(user);
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
            std::string session_user = session_manager_->getUsername(connection_id);
            if (!session_user.empty()) {
                username = session_user;
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
            session_manager_->updateHeartbeat(conn_id, version);
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

// cleanupInactiveSessions implementation removed

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
            auto sessions = session_manager_->getAllSessions();
            using namespace std::chrono;
            auto now = system_clock::now();
            int timeout = ServerConfig::instance().heartbeat_timeout_seconds;
            for (const auto& s : sessions) {
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

void ChatRoomServer::handleWebSocketMessage(std::shared_ptr<TcpConnection> conn, const protocols::WebSocketFrame& frame) {
    if (frame.opcode == protocols::WebSocketOpcode::TEXT) {
        std::string payload(frame.payload.begin(), frame.payload.end());
        try {
            auto j = json::parse(payload);
            std::string type = j.value("type", "");
            
            if (type == "login") {
                std::string username = j.value("username", "");
                std::string password = j.value("password", "");
                
                if (validateUsername(username)) {
                    // Verify password
                    if (!DatabaseManager::instance().validateUser(username, password)) {
                        json resp;
                        resp["type"] = "login_response";
                        resp["success"] = false;
                        resp["error"] = "Invalid username or password";
                        std::string respStr = resp.dump();
                        auto frameData = protocols::WebSocketCodec::buildFrame(protocols::WebSocketOpcode::TEXT, respStr);
                        conn->send(std::string(frameData.begin(), frameData.end()));
                        LOG_WARN("WS Login failed for {}: invalid credentials", username);
                        return;
                    }

                    {
                        std::lock_guard<std::mutex> lock(ws_mutex_);
                        ws_connections_[conn->name()] = username;
                        user_connections_[username] = conn;
                    }
                    
                    json resp;
                    resp["type"] = "login_response";
                    resp["success"] = true;
                    resp["username"] = username;
                    resp["user_id"] = DatabaseManager::instance().getUserId(username);
                    
                    std::string respStr = resp.dump();
                    auto frameData = protocols::WebSocketCodec::buildFrame(protocols::WebSocketOpcode::TEXT, respStr);
                    conn->send(std::string(frameData.begin(), frameData.end()));
                    LOG_INFO("WS User login: {}", username);
                }
            } else if (type == "join_room") {
                std::string room_id = j.value("room_id", "");
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(ws_mutex_);
                    auto it = ws_connections_.find(conn->name());
                    if (it != ws_connections_.end()) {
                        username = it->second;
                    }
                }
                
                if (!username.empty() && !room_id.empty()) {
                    {
                        std::lock_guard<std::mutex> lock(ws_mutex_);
                        room_members_[room_id].insert(username);
                    }
                    LOG_INFO("User {} joined room {}", username, room_id);
                }
            } else if (type == "leave_room") {
                std::string room_id = j.value("room_id", "");
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(ws_mutex_);
                    auto it = ws_connections_.find(conn->name());
                    if (it != ws_connections_.end()) {
                        username = it->second;
                    }
                }
                
                if (!username.empty() && !room_id.empty()) {
                    {
                        std::lock_guard<std::mutex> lock(ws_mutex_);
                        room_members_[room_id].erase(username);
                        if (room_members_[room_id].empty()) {
                            room_members_.erase(room_id);
                        }
                    }
                    LOG_INFO("User {} left room {}", username, room_id);
                }
            } else if (type == "message") {
                std::string content = j.value("content", "");
                std::string target = j.value("target_user", "");
                std::string room = j.value("room_id", "");
                
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(ws_mutex_);
                    auto it = ws_connections_.find(conn->name());
                    if (it != ws_connections_.end()) {
                        username = it->second;
                    }
                }
                
                if (!username.empty() && validateMessage(content)) {
                    ChatMessage msg;
                    msg.username = username;
                    msg.content = content;
                    msg.timestamp = getCurrentTimestamp();
                    msg.target_user = target;
                    msg.room_id = room;
                    
                    if (DatabaseManager::instance().addMessage(msg)) {
                        metrics_collector_->updateMessageCount(DatabaseManager::instance().getMessageCount());
                    }
                    
                    // Forwarding Logic
                    json forward_msg;
                    forward_msg["type"] = "message"; 
                    forward_msg["username"] = username;
                    forward_msg["content"] = content;
                    forward_msg["timestamp"] = msg.timestamp;
                    if (!target.empty()) forward_msg["target_user"] = target;
                    if (!room.empty()) forward_msg["room_id"] = room;
                    
                    std::string forwardStr = forward_msg.dump();
                    auto forwardFrame = protocols::WebSocketCodec::buildFrame(protocols::WebSocketOpcode::TEXT, forwardStr);
                    std::string forwardData(forwardFrame.begin(), forwardFrame.end());

                    {
                        std::lock_guard<std::mutex> lock(ws_mutex_);
                        if (!target.empty()) {
                            // Private message
                            auto it = user_connections_.find(target);
                            if (it != user_connections_.end()) {
                                it->second->send(forwardData);
                            }
                        } else if (!room.empty()) {
                            // Room Broadcast
                            auto it = room_members_.find(room);
                            if (it != room_members_.end()) {
                                for (const auto& member : it->second) {
                                    if (member != username) {
                                        auto user_it = user_connections_.find(member);
                                        if (user_it != user_connections_.end()) {
                                            user_it->second->send(forwardData);
                                        }
                                    }
                                }
                            }
                        } else {
                            // Global Broadcast (to all except sender)
                            for (auto& pair : user_connections_) {
                                if (pair.first != username) {
                                    pair.second->send(forwardData);
                                }
                            }
                        }
                    }
                    
                    // Echo confirmation
                    json resp;
                    resp["type"] = "message_response";
                    resp["success"] = true;
                    std::string respStr = resp.dump();
                    auto frameData = protocols::WebSocketCodec::buildFrame(protocols::WebSocketOpcode::TEXT, respStr);
                    conn->send(std::string(frameData.begin(), frameData.end()));
                    
                    LOG_INFO("WS Message from {}: {}", username, content);
                }
            }
        } catch (...) {
            LOG_ERROR("WS JSON parse error");
        }
    } else if (frame.opcode == protocols::WebSocketOpcode::CLOSE) {
        std::lock_guard<std::mutex> lock(ws_mutex_);
        auto it = ws_connections_.find(conn->name());
        if (it != ws_connections_.end()) {
            std::string username = it->second;
            user_connections_.erase(username);
            ws_connections_.erase(it);
            
            // Remove from all rooms
            for (auto& pair : room_members_) {
                pair.second.erase(username);
            }
            // Cleanup empty rooms
             for (auto it_room = room_members_.begin(); it_room != room_members_.end(); ) {
                if (it_room->second.empty()) {
                    it_room = room_members_.erase(it_room);
                } else {
                    ++it_room;
                }
            }
        }
    }
}

void ChatRoomServer::handleRtspMessage(std::shared_ptr<TcpConnection> conn, const protocols::RtspRequest& request) {
    protocols::RtspResponse response;
    response.cseq = request.cseq;
    
    if (request.method == protocols::RtspMethod::OPTIONS) {
        response.status_code = 200;
        response.status_text = "OK";
        response.headers["Public"] = "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE";
    } else if (request.method == protocols::RtspMethod::DESCRIBE) {
        response.status_code = 200;
        response.status_text = "OK";
        response.headers["Content-Type"] = "application/sdp";
        
        // Simple SDP
        std::string sdp = "v=0\r\n"
                          "o=- 0 0 IN IP4 127.0.0.1\r\n"
                          "s=ChatRoom Audio Session\r\n"
                          "c=IN IP4 127.0.0.1\r\n"
                          "t=0 0\r\n"
                          "m=audio 0 RTP/AVP 0\r\n"
                          "a=control:track0\r\n";
        response.body = sdp;
    } else {
        response.status_code = 501;
        response.status_text = "Not Implemented";
    }
    
    std::string resp_str = protocols::RtspCodec::buildResponse(response);
    conn->send(resp_str);
}

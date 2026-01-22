#include "chat_service.h"
#include "chatroom/session_manager.h"
#include "utils/json_utils.h"
#include "utils/server_error.h"
#include "database_manager.h"
#include "chat_message.h"
#include "utils/server_config.h"
#include "logger.h"
#include "net/tcp_connection.h"
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace {
std::string extractSipUsername(const std::string& header_value) {
    std::string username = header_value;
    if (username.find("sip:") == 0) {
        size_t at = username.find('@');
        if (at != std::string::npos) {
            username = username.substr(4, at - 4);
        } else {
            username = username.substr(4);
        }
    }
    
    size_t semi = username.find(';');
    if (semi != std::string::npos) {
        username = username.substr(0, semi);
    }
    
    // Trim whitespace
    username.erase(0, username.find_first_not_of(" \t"));
    username.erase(username.find_last_not_of(" \t") + 1);
    
    return username;
}

std::string getQueryParam(const std::string& path, const std::string& key) {
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
}

ChatService::ChatService(std::shared_ptr<MetricsCollector> metrics,
                         SessionManager* session_manager)
    : metrics_collector_(std::move(metrics)),
      session_manager_(session_manager),
      rate_limiter_() {
}

bool ChatService::checkRateLimit(const std::string& ip) {
    return rate_limiter_.check(ip);
}

bool ChatService::validateUsername(const std::string& username) {
    if (username.empty() || username.length() > ServerConfig::instance().max_username_length) return false;
    for (char c : username) {
        if (!isalnum(c) && c != '_') return false;
    }
    return true;
}

bool ChatService::validateMessage(const std::string& content) {
    if (content.empty() || content.length() > ServerConfig::instance().max_message_length) return false;
    for (char c : content) {
        if (iscntrl(c) && c != '\n' && c != '\t') return false;
    }
    return true;
}

bool ChatService::sendUserMessage(const std::string& username,
                                  const std::string& content,
                                  const std::string& target_user,
                                  const std::string& room_id) {
    if (!validateUsername(username)) {
        return false;
    }
    if (!validateMessage(content)) {
        return false;
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
        return true;
    } else {
        LOG_ERROR("Failed to store message to database");
        return false;
    }
}

void ChatService::handleSipMessage(std::shared_ptr<TcpConnection> conn, const SipRequest& request, const std::string& raw_msg) {
    std::string method = SipCodec::methodToString(request.method);
    LOG_INFO("Handling SIP {} from {}", method, request.headers.count("From") ? request.headers.at("From") : "unknown");

    if (request.method == SipMethod::REGISTER) {
        if (request.headers.count("From")) {
            std::string username = extractSipUsername(request.headers.at("From"));
            
            // Register in SessionManager
            session_manager_->registerSipSession(username, conn);
            LOG_INFO("SIP User registered: {}", username);
            
            // Send 200 OK
            std::string response = SipCodec::buildResponse(200, "OK", request);
            conn->send(response);
        }
    } else if (request.method == SipMethod::INVITE) {
        if (request.headers.count("To")) {
             std::string target_user = extractSipUsername(request.headers.at("To"));

             auto target_conn = session_manager_->getSipConnection(target_user);
             if (target_conn) {
                 // Forward the raw INVITE message
                 LOG_INFO("Forwarding INVITE to user: {}", target_user);
                 target_conn->send(raw_msg);
             } else {
                 LOG_WARN("SIP User not found: {}", target_user);
                 std::string response = SipCodec::buildResponse(404, "Not Found", request);
                 conn->send(response);
             }
        }
    } else {
         // Forward other methods if they have a To header and we know the target
         if (request.headers.count("To")) {
             std::string target_user = extractSipUsername(request.headers.at("To"));
             auto target_conn = session_manager_->getSipConnection(target_user);
             if (target_conn) {
                 target_conn->send(raw_msg);
                 return;
             }
         }
         
         // Default response if not forwarded
         if (request.method == SipMethod::OPTIONS) {
             std::string response = SipCodec::buildResponse(200, "OK", request);
             conn->send(response);
         }
    }
 }
 
 void ChatService::handleFtpMessage(std::shared_ptr<TcpConnection> conn, const std::string& command) {
    LOG_INFO("Handling FTP command: {}", command);
    std::string response = "500 Unknown command\r\n";
    
    if (command.find("USER") == 0) {
        response = "331 User name okay, need password.\r\n";
    } else if (command.find("PASS") == 0) {
        response = "230 User logged in, proceed.\r\n";
    } else if (command.find("QUIT") == 0) {
        response = "221 Service closing control connection.\r\n";
        conn->send(response);
        conn->setCloseAfterWrite(true);
        return;
    } else if (command.find("PWD") == 0) {
        response = "257 \"/\" is the current directory\r\n";
    } else if (command.find("SYST") == 0) {
        response = "215 UNIX Type: L8\r\n";
    } else if (command.find("FEAT") == 0) {
        response = "211-Features:\r\n SIZE\r\n211 End\r\n";
    } else {
        response = "502 Command not implemented.\r\n";
    }
    
    conn->send(response);
}

 std::string ChatService::getCurrentTimestamp() {
    return formatTimestamp(std::chrono::system_clock::now());
}

std::string ChatService::formatTimestamp(const std::chrono::system_clock::time_point& tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

HttpResponse ChatService::handleLogin(const HttpRequest& request) {
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
        
        auto result = session_manager_->login(username);
        if (!result.success) {
            return CreateErrorResponse(ErrorCode::USERNAME_TAKEN);
        }
        
        LOG_INFO("用户登录: {} (conn_id={})", username, result.connection_id);
        
        json resp_json;
        resp_json["success"] = true;
        resp_json["connection_id"] = result.connection_id;
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

HttpResponse ChatService::handleGetUsers(const HttpRequest& request) {
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

HttpResponse ChatService::handleSendMessage(const HttpRequest& request) {
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
        if (!sendUserMessage(username, content, target_user, room_id)) {
            return CreateErrorResponse(ErrorCode::INTERNAL_ERROR);
        }

        LOG_INFO("收到消息 [{}]: {}", username, content);
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

HttpResponse ChatService::handleGetMessages(const HttpRequest& request) {
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

HttpResponse ChatService::handleHeartbeat(const HttpRequest& request) {
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

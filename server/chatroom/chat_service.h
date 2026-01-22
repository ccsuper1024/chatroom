#pragma once

#include <memory>
#include <string>
#include <chrono>
#include "http/http_codec.h"
#include "utils/metrics_collector.h"
#include "utils/rate_limiter.h"
#include "sip/sip_codec.h"

class SessionManager;
class TcpConnection;

class ChatService {
public:
    ChatService(std::shared_ptr<MetricsCollector> metrics,
                SessionManager* session_manager);

    HttpResponse handleLogin(const HttpRequest& request);
    HttpResponse handleSendMessage(const HttpRequest& request);
    HttpResponse handleGetMessages(const HttpRequest& request);
    HttpResponse handleGetUsers(const HttpRequest& request);
    HttpResponse handleHeartbeat(const HttpRequest& request);

    // SIP handling
    void handleSipMessage(std::shared_ptr<TcpConnection> conn, const SipRequest& request, const std::string& raw_msg);

    // FTP handling
    void handleFtpMessage(std::shared_ptr<TcpConnection> conn, const std::string& command);

    bool sendUserMessage(const std::string& username,
                         const std::string& content,
                         const std::string& target_user,
                         const std::string& room_id);

private:
    bool checkRateLimit(const std::string& ip);
    bool validateUsername(const std::string& username);
    bool validateMessage(const std::string& content);
    std::string getCurrentTimestamp();
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);

    std::shared_ptr<MetricsCollector> metrics_collector_;
    SessionManager* session_manager_;
    RateLimiter rate_limiter_;
};

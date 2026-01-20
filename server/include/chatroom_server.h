#pragma once

#include "http_server.h"
#include "metrics_collector.h"
#include "server_error.h"
#include "chat_message.h"
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <ratio>
#include <thread>
#include <atomic>
#include <condition_variable>

/**
 * 聊天室服务器
 */
class ChatRoomServer {
public:
    friend class ChatRoomServerTest;
    explicit ChatRoomServer(int port);
    
    // 启动聊天室服务器
    void start();
    
    // 停止聊天室服务器
    void stop();

private:
    std::unique_ptr<HttpServer> http_server_;
    std::shared_ptr<MetricsCollector> metrics_collector_;
    
    struct UserSession {
        std::string username;
        std::string connection_id;
        std::string client_version;
        std::chrono::system_clock::time_point last_heartbeat;
        std::chrono::system_clock::time_point login_time;
    };
    std::unordered_map<std::string, UserSession> sessions_;
    std::mutex sessions_mutex_;
    std::chrono::system_clock::time_point start_time_;
    std::atomic<bool> running_;
    std::thread cleanup_thread_;
    std::mutex cleanup_mutex_;
    std::condition_variable cleanup_cv_;
    
    // 处理用户登录
    HttpResponse handleLogin(const HttpRequest& request);
    
    // 处理发送消息
    HttpResponse handleSendMessage(const HttpRequest& request);
    
    // 处理获取消息
    HttpResponse handleGetMessages(const HttpRequest& request);
    
    HttpResponse handleGetUsers(const HttpRequest& request);
    
    HttpResponse handleMetrics(const HttpRequest& request);

    std::string getCurrentTimestamp();
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);
    
    HttpResponse handleHeartbeat(const HttpRequest& request);
    
    void cleanupInactiveSessions();
    
    // 安全与限流
    struct RateLimitEntry {
        int count;
        std::chrono::steady_clock::time_point reset_time;
    };
    std::unordered_map<std::string, RateLimitEntry> rate_limits_;
    std::mutex rate_limit_mutex_;
    
    bool checkRateLimit(const std::string& ip);
    bool validateUsername(const std::string& username);
    bool validateMessage(const std::string& content);
};

#pragma once

#include "http/http_server.h"
#include "utils/metrics_collector.h"
#include "utils/server_error.h"
#include "utils/rate_limiter.h"
#include "chatroom/session_manager.h"
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
    std::unique_ptr<SessionManager> session_manager_;
    
    std::chrono::system_clock::time_point start_time_;
    std::atomic<bool> running_;
    
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
    
    // 安全与限流
    RateLimiter rate_limiter_;
    
    bool checkRateLimit(const std::string& ip);
    bool validateUsername(const std::string& username);
    bool validateMessage(const std::string& content);

    // WebSocket Handling
    void handleWebSocketMessage(std::shared_ptr<TcpConnection> conn, const protocols::WebSocketFrame& frame);
    std::unordered_map<int, std::string> ws_connections_; // fd -> username
    std::mutex ws_mutex_;

    // RTSP Handling
    void handleRtspMessage(std::shared_ptr<TcpConnection> conn, const protocols::RtspRequest& request);
};

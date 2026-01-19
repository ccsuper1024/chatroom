#pragma once

#include "http_server.h"
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
 * 聊天消息结构
 */
struct ChatMessage {
    std::string username;
    std::string content;
    std::string timestamp;
};

/**
 * 聊天室服务器
 */
class ChatRoomServer {
public:
    explicit ChatRoomServer(int port);
    
    // 启动聊天室服务器
    void start();
    
    // 停止聊天室服务器
    void stop();

private:
    std::unique_ptr<HttpServer> http_server_;
    std::deque<ChatMessage> messages_;  // 消息历史
    std::size_t base_message_index_;    // 历史消息的起始索引
    std::mutex messages_mutex_;          // 保护消息列表的互斥锁
    
    struct UserSession {
        std::string username;
        std::string connection_id;
        std::string client_version;
        std::chrono::system_clock::time_point last_heartbeat;
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
};

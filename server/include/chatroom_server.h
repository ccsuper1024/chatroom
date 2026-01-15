#pragma once

#include "http_server.h"
#include <string>
#include <vector>
#include <mutex>
#include <memory>

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
    std::vector<ChatMessage> messages_;  // 消息历史
    std::mutex messages_mutex_;          // 保护消息列表的互斥锁
    
    // 处理用户登录
    HttpResponse handleLogin(const HttpRequest& request);
    
    // 处理发送消息
    HttpResponse handleSendMessage(const HttpRequest& request);
    
    // 处理获取消息
    HttpResponse handleGetMessages(const HttpRequest& request);
    
    HttpResponse handleGetUsers(const HttpRequest& request);
    
    std::string getCurrentTimestamp();
    
    HttpResponse handleHeartbeat(const HttpRequest& request);
};

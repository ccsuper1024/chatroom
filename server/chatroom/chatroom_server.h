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

class EventLoop;
class TcpConnection;
class ChatService;

/**
 * @brief 聊天室服务器主类
 * 
 * 负责管理HTTP服务器、会话管理器以及处理各类业务请求（登录、消息、用户列表等）。
 * 同时集成了WebSocket和RTSP协议的处理入口。
 */
class ChatRoomServer {
public:
    friend class ChatRoomServerTest;
    
    /**
     * @brief 构造函数
     * @param port 服务器监听端口
     */
    explicit ChatRoomServer(int port);
    
    /**
     * @brief 析构函数
     */
    ~ChatRoomServer();

    /**
     * @brief 启动聊天室服务器
     * 
     * 初始化数据库、启动SessionManager和HttpServer。
     */
    void start();
    
    /**
     * @brief 停止聊天室服务器
     * 
     * 停止HttpServer并清理资源。
     */
    void stop();

    EventLoop* getLoop() const { return const_cast<EventLoop*>(&loop_); }

private:
    EventLoop loop_;
    std::unique_ptr<HttpServer> http_server_;           ///< HTTP服务器实例

    std::shared_ptr<MetricsCollector> metrics_collector_; ///< 指标收集器
    std::unique_ptr<SessionManager> session_manager_;   ///< 会话管理器
    std::unique_ptr<ChatService> chat_service_;         ///< 聊天业务服务
    
    std::chrono::system_clock::time_point start_time_;  ///< 服务器启动时间
    std::atomic<bool> running_;                         ///< 运行状态标志
    
    /**
     * @brief 处理用户登录请求
     * @param request HTTP请求对象
     * @return HttpResponse HTTP响应对象
     */
    HttpResponse handleLogin(const HttpRequest& request);
    
    /**
     * @brief 处理发送消息请求
     * @param request HTTP请求对象
     * @return HttpResponse HTTP响应对象
     */
    HttpResponse handleSendMessage(const HttpRequest& request);
    
    /**
     * @brief 处理获取消息列表请求
     * @param request HTTP请求对象
     * @return HttpResponse HTTP响应对象
     */
    HttpResponse handleGetMessages(const HttpRequest& request);
    
    /**
     * @brief 处理获取用户列表请求
     * @param request HTTP请求对象
     * @return HttpResponse HTTP响应对象
     */
    HttpResponse handleGetUsers(const HttpRequest& request);
    
    /**
     * @brief 处理指标获取请求
     * @param request HTTP请求对象
     * @return HttpResponse HTTP响应对象
     */
    HttpResponse handleMetrics(const HttpRequest& request);

    /**
     * @brief 获取当前时间戳字符串
     * @return 格式化的时间字符串
     */
    std::string getCurrentTimestamp();

    /**
     * @brief 格式化指定时间点
     * @param tp 时间点
     * @return 格式化的时间字符串
     */
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp);
    
    /**
     * @brief 处理心跳请求
     * @param request HTTP请求对象
     * @return HttpResponse HTTP响应对象
     */
    HttpResponse handleHeartbeat(const HttpRequest& request);
    
    // 安全与限流
    RateLimiter rate_limiter_; ///< 限流器实例
    
    /**
     * @brief 检查IP是否触发限流
     * @param ip 客户端IP地址
     * @return true 未触发限流，允许访问
     * @return false 触发限流，拒绝访问
     */
    bool checkRateLimit(const std::string& ip);

    /**
     * @brief 验证用户名合法性
     * @param username 用户名
     * @return true 合法
     * @return false 非法
     */
    bool validateUsername(const std::string& username);

    /**
     * @brief 验证消息内容合法性
     * @param content 消息内容
     * @return true 合法
     * @return false 非法
     */
    bool validateMessage(const std::string& content);

    // WebSocket Handling
    /**
     * @brief 处理WebSocket消息
     * @param conn TCP连接对象
     * @param frame WebSocket数据帧
     */
    void handleWebSocketMessage(std::shared_ptr<TcpConnection> conn, const protocols::WebSocketFrame& frame);
    std::unordered_map<std::string, std::string> ws_connections_; ///< WebSocket连接映射 (conn_name -> username)
    std::mutex ws_mutex_;                                 ///< WebSocket连接映射互斥锁

    // RTSP Handling
    /**
     * @brief 处理RTSP请求
     * @param conn TCP连接对象
     * @param request RTSP请求对象
     */
    void handleRtspMessage(std::shared_ptr<TcpConnection> conn, const protocols::RtspRequest& request);
};

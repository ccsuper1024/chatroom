#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "net/tcp_server.h"
#include "net/event_loop.h"
#include "http/http_codec.h"
#include "utils/thread_pool.h"
#include "websocket/websocket_codec.h"

class TcpConnection;

/**
 * @brief HTTP请求处理函数类型
 */
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

/**
 * @brief WebSocket消息处理函数类型
 */
using WebSocketHandler = std::function<void(const TcpConnectionPtr&, const protocols::WebSocketFrame&)>;

/**
 * @brief HTTP服务器核心类
 * 
 * 负责监听端口、接受连接、分发请求到对应的处理器。
 * 支持HTTP和WebSocket协议。
 */
class HttpServer {
public:
    friend class ChatRoomServerTest;

    /**
     * @brief 构造函数
     * @param port 服务器监听端口
     */
    explicit HttpServer(EventLoop* loop, int port);
    
    /**
     * @brief 析构函数
     * 
     * 停止服务器并释放资源。
     */
    ~HttpServer();

    /**
     * @brief 注册HTTP路由处理器
     * @param path 请求路径 (例如 "/api/login")
     * @param handler 处理函数
     */
    void registerHandler(const std::string& path, HttpHandler handler);

    /**
     * @brief 设置WebSocket消息处理器
     * @param handler 处理函数
     */
    void setWebSocketHandler(WebSocketHandler handler);
    
    /**
     * @brief 设置静态资源目录
     * @param dir 静态资源根目录路径 (例如 "wwwroot")
     */
    void setStaticResourceDir(const std::string& dir);

    /**
     * @brief 启动服务器
     * 
     * 初始化Acceptor并进入事件循环（阻塞）。
     */
    void start();
    
    /**
     * @brief 停止服务器
     */
    void stop();

    /**
     * @brief 获取监听端口
     */
    int port() const { return port_; }
    
    EventLoop* getLoop() const { return server_.getLoop(); }

    // 线程池指标
    /**
     * @brief 获取业务线程池任务队列大小
     * @return 队列中等待的任务数量
     */
    std::size_t getThreadPoolQueueSize() const;
    
    /**
     * @brief 获取业务线程池拒绝任务数
     * @return 被拒绝的任务总数
     */
    std::size_t getThreadPoolRejectedCount() const;

    /**
     * @brief 获取业务线程池线程总数
     * @return 线程池大小
     */
    std::size_t getThreadPoolThreadCount() const;
    
    /**
     * @brief 获取业务线程池活跃线程数
     * @return 正在执行任务的线程数量
     */
    std::size_t getThreadPoolActiveThreadCount() const;

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime);
    void onRequest(const TcpConnectionPtr& conn, const HttpRequest& req);

    TcpServer server_;
    int port_;
    std::map<std::string, HttpHandler> handlers_;
    ThreadPool thread_pool_;
    
    WebSocketHandler ws_handler_;
    std::string static_resource_dir_;
    
    /**
     * @brief 处理静态文件请求
     * @param path 文件相对路径
     * @return HTTP响应
     */
    HttpResponse serveStaticFile(const std::string& path);
    
    std::string buildResponse(const HttpResponse& resp);
};


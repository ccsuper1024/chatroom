#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "net/event_loop.h"
#include "net/event_loop_thread_pool.h"
#include "http/http_codec.h"
#include "utils/thread_pool.h"
#include "websocket/websocket_codec.h"
#include "rtsp/rtsp_codec.h"

class Acceptor;
class TcpConnection;

// 定义请求处理函数类型
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;
using WebSocketHandler = std::function<void(std::shared_ptr<TcpConnection>, const protocols::WebSocketFrame&)>;
using RtspHandler = std::function<void(std::shared_ptr<TcpConnection>, const protocols::RtspRequest&)>;

class HttpServer {
public:
    explicit HttpServer(int port);
    ~HttpServer();

    // 注册路由处理器
    void registerHandler(const std::string& path, HttpHandler handler);

    // 设置 WebSocket 处理器
    void setWebSocketHandler(WebSocketHandler handler);
    
    // Set RTSP Handler
    void setRtspHandler(RtspHandler handler);

    // 启动服务器（阻塞）
    void start();
    
    // 停止服务器
    void stop();

    // 线程池指标
    std::size_t getThreadPoolQueueSize() const;
    std::size_t getThreadPoolRejectedCount() const;
    std::size_t getThreadPoolThreadCount() const;
    std::size_t getThreadPoolActiveThreadCount() const;

private:
    friend class TcpConnection;

    int port_;
    bool running_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<EventLoopThreadPool> io_thread_pool_;
    
    std::map<int, std::shared_ptr<TcpConnection>> connections_;
    
    void handleHttpRequest(std::shared_ptr<TcpConnection> conn, const HttpRequest& request);
    void handleWebSocketMessage(std::shared_ptr<TcpConnection> conn, const protocols::WebSocketFrame& frame);
    void handleRtspMessage(std::shared_ptr<TcpConnection> conn, const protocols::RtspRequest& request);

    void newConnection(int fd, const std::string& ip);
    void closeConnection(int fd);
    
    // 路由表
    std::map<std::string, HttpHandler> handlers_;
    WebSocketHandler ws_handler_;
    RtspHandler rtsp_handler_;

    EventLoop loop_;
    std::unique_ptr<Acceptor> acceptor_;
};

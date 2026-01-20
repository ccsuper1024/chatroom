#pragma once

#include <string>
#include <functional>
#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include <cstddef>
#include "http_codec.h"
#include "event_loop.h"
#include "channel.h"
#include "acceptor.h"


/**
 * HTTP请求处理器回调函数类型
 */
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

class ThreadPool;
class TcpConnection;

class EventLoopThreadPool;

class HttpServer {
public:
    explicit HttpServer(int port);
    ~HttpServer();
    
    // 注册路由处理器
    void registerHandler(const std::string& path, HttpHandler handler);
    
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
    void newConnection(int fd, const std::string& ip);
    void closeConnection(int fd);
    
    // 路由表
    std::map<std::string, HttpHandler> handlers_;

    EventLoop loop_;
    std::unique_ptr<Acceptor> acceptor_;
};

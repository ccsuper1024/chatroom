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

struct ConnectionCheckConfig {
    int check_interval_seconds;
    int max_failures;
    std::size_t thread_pool_core;
    std::size_t thread_pool_max;
    std::size_t thread_queue_capacity;
};

/**
 * HTTP请求处理器回调函数类型
 */
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

class ThreadPool;
class TcpConnection;

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

private:
    friend class TcpConnection;

    int port_;
    bool running_;
    ConnectionCheckConfig conn_cfg_;
    std::unique_ptr<ThreadPool> thread_pool_;
    
    std::map<int, std::unique_ptr<TcpConnection>> connections_;
    
    struct PendingResponse {
        int fd;
        std::string data;
    };
    std::mutex pending_mutex_;
    std::vector<PendingResponse> pending_responses_;
    
    void handleHttpRequest(int fd, const HttpRequest& request);
    void newConnection(int fd);
    void closeConnection(int fd);
    void processPendingResponses();
    
    // 路由表
    std::map<std::string, HttpHandler> handlers_;

    EventLoop loop_;
    std::unique_ptr<Acceptor> acceptor_;
};

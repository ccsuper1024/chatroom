#include "http_server.h"
#include "logger.h"
#include "thread_pool.h"
#include "http_codec.h"
#include "tcp_connection.h"
#include "server_config.h"
#include "event_loop_thread_pool.h"
#include "server_error.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <map>
#include <fstream>
#include <algorithm>
#include <cctype>

HttpServer::HttpServer(int port) 
    : port_(port),
      running_(false) {
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::registerHandler(const std::string& path, HttpHandler handler) {
    handlers_[path] = handler;
    LOG_INFO("注册路由: {}", path);
}

void HttpServer::start() {
    const auto& poolCfg = ServerConfig::instance().thread_pool;
    thread_pool_ = std::make_unique<ThreadPool>(
        poolCfg.core_threads,
        poolCfg.max_threads,
        poolCfg.queue_capacity);
    
    io_thread_pool_ = std::make_unique<EventLoopThreadPool>(&loop_);
    io_thread_pool_->setThreadNum(4); // Default to 4 IO threads
    io_thread_pool_->start();

    acceptor_ = std::make_unique<Acceptor>(&loop_, port_);
    if (!acceptor_->isValid()) {
        LOG_ERROR("Acceptor 初始化失败，HTTP服务器启动失败");
        acceptor_.reset();
        return;
    }

    acceptor_->setNewConnectionCallback([this](int client_fd, const std::string& ip) {
        newConnection(client_fd, ip);
    });

    running_ = true;
    LOG_INFO("HTTP服务器启动，监听端口: {}", port_);
    loop_.loop();
}

void HttpServer::stop() {
    running_ = false;
    loop_.stop();
    if (acceptor_) {
        acceptor_.reset();
    }
    
    // Clear connections
    for (auto& kv : connections_) {
        auto& conn = kv.second;
        if (conn && !conn->closed()) {
            conn->shutdown();
        }
    }
    connections_.clear();
    LOG_INFO("HTTP服务器已停止");
}

void HttpServer::newConnection(int client_fd, const std::string& ip) {
    EventLoop* ioLoop = io_thread_pool_->getNextLoop();
    auto conn = std::make_shared<TcpConnection>(this, ioLoop, client_fd, ip);
    connections_[client_fd] = conn;
    conn->connectEstablished();
}

void HttpServer::closeConnection(int fd) {
    loop_.runInLoop([this, fd]() {
        auto it = connections_.find(fd);
        if (it != connections_.end()) {
            connections_.erase(it);
        }
    });
}

void HttpServer::handleHttpRequest(std::shared_ptr<TcpConnection> conn, const HttpRequest& request) {
    auto task = [this, conn, request]() {
        HttpResponse response;
        
        std::string path = request.path;
        auto q_pos = path.find('?');
        if (q_pos != std::string::npos) {
            path = path.substr(0, q_pos);
        }
        
        auto it = handlers_.find(path);
        if (it != handlers_.end()) {
            response = it->second(request);
        } else {
            response.status_code = 404;
            response.status_text = "Not Found";
            response.body = "{\"error\":\"Not Found\"}";
        }
        
        std::string resp_str = buildResponse(response);
        conn->send(resp_str);
    };

    if (thread_pool_) {
        bool posted = thread_pool_->tryPost(task);
        if (!posted) {
            LOG_WARN("线程池满，丢弃请求");
            HttpResponse resp = CreateErrorResponse(ErrorCode::SERVER_BUSY);
            conn->send(buildResponse(resp));
        }
    } else {
        task();
    }
}

std::size_t HttpServer::getThreadPoolQueueSize() const {
    return thread_pool_ ? thread_pool_->queueSize() : 0;
}

std::size_t HttpServer::getThreadPoolRejectedCount() const {
    return thread_pool_ ? thread_pool_->rejectedCount() : 0;
}

std::size_t HttpServer::getThreadPoolThreadCount() const {
    return thread_pool_ ? thread_pool_->currentThreadCount() : 0;
}

std::size_t HttpServer::getThreadPoolActiveThreadCount() const {
    return thread_pool_ ? thread_pool_->activeThreadCount() : 0;
}

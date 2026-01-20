#include "http_server.h"
#include "logger.h"
#include "thread_pool.h"
#include "http_codec.h"
#include "tcp_connection.h"
#include "server_config.h"
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
    loop_.loop([this]() { processPendingResponses(); });
}

void HttpServer::stop() {
    running_ = false;
    loop_.stop();
    if (acceptor_) {
        acceptor_.reset();
    }
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
    auto conn = std::make_unique<TcpConnection>(this, &loop_, client_fd, ip);
    
    // TcpConnection manages its own callbacks and parsing
    
    connections_[client_fd] = std::move(conn);
}

void HttpServer::closeConnection(int fd) {
    auto it = connections_.find(fd);
    if (it != connections_.end()) {
        connections_.erase(it);
    }
}

void HttpServer::handleHttpRequest(int fd, const HttpRequest& request) {
    auto task = [this, fd, request]() {
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
        
        std::string resp_str = "HTTP/1.1 " + std::to_string(response.status_code) + " " + response.status_text + "\r\n";
        resp_str += "Content-Type: application/json\r\n";
        resp_str += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
        resp_str += "Connection: keep-alive\r\n";
        resp_str += "Access-Control-Allow-Origin: *\r\n"; // CORS for web clients if any
        resp_str += "\r\n";
        resp_str += response.body;
        
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_responses_.push_back({fd, resp_str});
        loop_.wakeup();
    };

    if (thread_pool_) {
        bool posted = thread_pool_->tryPost(task);
        if (!posted) {
            LOG_WARN("线程池满，丢弃请求");
            // Send 503 Service Unavailable directly? 
            // Better to queue a 503 response if possible, but we are in IO thread here (if called from TcpConnection)
            // or we are in... wait. handleHttpRequest is called from TcpConnection::handleRead -> EventLoop thread.
            // So we can safely push to pending_responses_ directly or just send?
            // But we want to be consistent.
            
            std::string resp = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n";
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_responses_.push_back({fd, resp});
            loop_.wakeup();
        }
    } else {
        // Fallback to sync execution if no thread pool
        task();
    }
}

void HttpServer::processPendingResponses() {
    std::vector<PendingResponse> responses;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        responses.swap(pending_responses_);
    }
    
    for (const auto& resp : responses) {
        auto it = connections_.find(resp.fd);
        if (it != connections_.end()) {
            it->second->appendResponse(resp.data);
        }
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

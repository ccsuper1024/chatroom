#include "http_server.h"
#include "logger.h"
#include "thread_pool.h"
#include "http_codec.h"
#include "tcp_connection.h"
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

static std::string trim(const std::string& s) {
    auto begin = s.begin();
    while (begin != s.end() && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    auto end = s.end();
    if (begin == end) {
        return std::string();
    }
    do {
        --end;
    } while (end != begin && std::isspace(static_cast<unsigned char>(*end)));
    return std::string(begin, end + 1);
}

static ConnectionCheckConfig loadConnectionCheckConfig() {
    ConnectionCheckConfig cfg{};
    cfg.check_interval_seconds = 30;
    cfg.max_failures = 3;
    cfg.thread_pool_core = 0;
    cfg.thread_pool_max = 0;
    cfg.thread_queue_capacity = 0;
    std::ifstream in("conf/server.yaml");
    if (!in.is_open()) {
        return cfg;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') {
            continue;
        }
        auto pos = t.find(':');
        if (pos == std::string::npos) {
            continue;
        }
        std::string key = trim(t.substr(0, pos));
        std::string value = trim(t.substr(pos + 1));
        try {
            if (key == "check_interval_seconds") {
                cfg.check_interval_seconds = std::stoi(value);
            } else if (key == "max_failures") {
                cfg.max_failures = std::stoi(value);
            } else if (key == "thread_pool_core") {
                cfg.thread_pool_core = static_cast<std::size_t>(std::stoul(value));
            } else if (key == "thread_pool_max") {
                cfg.thread_pool_max = static_cast<std::size_t>(std::stoul(value));
            } else if (key == "thread_queue_capacity") {
                cfg.thread_queue_capacity = static_cast<std::size_t>(std::stoul(value));
            }
        } catch (...) {
        }
    }
    if (cfg.check_interval_seconds <= 0) {
        cfg.check_interval_seconds = 30;
    }
    if (cfg.max_failures <= 0) {
        cfg.max_failures = 3;
    }
    
    std::size_t hw = std::thread::hardware_concurrency();
    if (hw == 0) {
        hw = 4;
    }
    if (cfg.thread_pool_core == 0) {
        cfg.thread_pool_core = std::max<std::size_t>(1, hw / 2);
    }
    if (cfg.thread_pool_max == 0) {
        cfg.thread_pool_max = std::max<std::size_t>(cfg.thread_pool_core, hw * 2);
    }
    if (cfg.thread_queue_capacity == 0) {
        cfg.thread_queue_capacity = 1024;
    }
    
    return cfg;
}

HttpServer::HttpServer(int port) 
    : port_(port),
      running_(false),
      conn_cfg_(loadConnectionCheckConfig()) {
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::registerHandler(const std::string& path, HttpHandler handler) {
    handlers_[path] = handler;
    LOG_INFO("注册路由: {}", path);
}

void HttpServer::start() {
    thread_pool_ = std::make_unique<ThreadPool>(
        conn_cfg_.thread_pool_core,
        conn_cfg_.thread_pool_max,
        conn_cfg_.thread_queue_capacity);
    
    acceptor_ = std::make_unique<Acceptor>(&loop_, port_);
    if (!acceptor_->isValid()) {
        LOG_ERROR("Acceptor 初始化失败，HTTP服务器启动失败");
        acceptor_.reset();
        return;
    }

    acceptor_->setNewConnectionCallback([this](int client_fd) {
        newConnection(client_fd);
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

void HttpServer::handleHttpRequest(int fd, const HttpRequest& request) {
    int request_fd = fd;
    HttpRequest request_copy = request;
    thread_pool_->post([this, request_fd, request_copy]() {
        HttpResponse response;
        
        std::string path = request_copy.path;
        size_t q_pos = path.find('?');
        if (q_pos != std::string::npos) {
            path = path.substr(0, q_pos);
        }

        auto it_handler = handlers_.find(path);
        if (it_handler != handlers_.end()) {
            response = it_handler->second(request_copy);
        } else {
            response.status_code = 404;
            response.status_text = "Not Found";
            response.body = R"({"error": "路径未找到"})";
        }
        std::string response_str = buildResponse(response);
        PendingResponse pr;
        pr.fd = request_fd;
        pr.data = std::move(response_str);
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_responses_.push_back(std::move(pr));
    });
}

void HttpServer::newConnection(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        close(fd);
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

#ifdef TCP_KEEPIDLE
    int idle = conn_cfg_.check_interval_seconds;
    if (idle <= 0) {
        idle = 30;
    }
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
#endif

#ifdef TCP_KEEPINTVL
    int intvl = conn_cfg_.check_interval_seconds;
    if (intvl <= 0) {
        intvl = 30;
    }
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
#endif

#ifdef TCP_KEEPCNT
    int cnt = conn_cfg_.max_failures;
    if (cnt <= 0) {
        cnt = 3;
    }
    setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#endif

    auto conn = std::make_unique<TcpConnection>(this, &loop_, fd);
    connections_[fd] = std::move(conn);
    LOG_INFO("新连接建立, fd={}", fd);
}

void HttpServer::closeConnection(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }
    auto& conn = it->second;
    if (conn && !conn->closed()) {
        conn->shutdown();
        LOG_INFO("连接关闭, fd={}", fd);
    }
    connections_.erase(it);
}

void HttpServer::processPendingResponses() {
    std::vector<PendingResponse> pending;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        if (pending_responses_.empty()) {
            return;
        }
        pending.swap(pending_responses_);
    }
    for (auto& pr : pending) {
        auto it = connections_.find(pr.fd);
        if (it == connections_.end()) {
            continue;
        }
        auto& conn = it->second;
        if (!conn || conn->closed()) {
            continue;
        }
        conn->appendResponse(pr.data);
    }
}

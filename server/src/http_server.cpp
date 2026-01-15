#include "http_server.h"
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <map>
#include <thread>

HttpServer::HttpServer(int port) 
    : port_(port), server_fd_(-1), running_(false) {
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::registerHandler(const std::string& path, HttpHandler handler) {
    handlers_[path] = handler;
    spdlog::info("注册路由: {}", path);
}

void HttpServer::start() {
    // 创建套接字
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        spdlog::error("创建套接字失败");
        return;
    }
    
    // 设置端口复用
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        spdlog::error("设置套接字选项失败");
        close(server_fd_);
        return;
    }
    
    // 绑定地址
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        spdlog::error("绑定端口 {} 失败", port_);
        close(server_fd_);
        return;
    }
    
    // 监听
    if (listen(server_fd_, 10) < 0) {
        spdlog::error("监听失败");
        close(server_fd_);
        return;
    }
    
    running_ = true;
    spdlog::info("HTTP服务器启动，监听端口: {}", port_);
    
    // 接受连接
    while (running_) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (running_) {
                spdlog::error("接受连接失败");
            }
            continue;
        }
        
        // 处理客户端请求
        // 使用线程处理客户端，支持并发和长连接
        std::thread([this, client_fd]() {
            try {
                this->handleClient(client_fd);
            } catch (...) {
                spdlog::error("处理客户端异常");
            }
            close(client_fd);
        }).detach();
    }
}

void HttpServer::stop() {
    running_ = false;
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    spdlog::info("HTTP服务器已停止");
}

void HttpServer::handleClient(int client_fd) {
    while (true) {
        char buffer[4096] = {0};
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            break;
        }
        
        std::string raw_request(buffer, bytes_read);
        spdlog::debug("收到请求:\n{}", raw_request);
        
        // 解析请求
        HttpRequest request = parseRequest(raw_request);
        
        // 查找处理器
        HttpResponse response;
        auto it = handlers_.find(request.path);
        if (it != handlers_.end()) {
            response = it->second(request);
        } else {
            response.status_code = 404;
            response.status_text = "Not Found";
            response.body = R"({"error": "路径未找到"})";
        }
        
        // 构建并发送响应
        std::string response_str = buildResponse(response);
        if (send(client_fd, response_str.c_str(), response_str.size(), MSG_NOSIGNAL) < 0) {
            break;
        }
    }
}

HttpRequest HttpServer::parseRequest(const std::string& raw_request) {
    HttpRequest request;
    std::istringstream stream(raw_request);
    std::string line;
    
    // 解析请求行
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> request.method >> request.path;
    }
    
    // 解析头部
    size_t content_length = 0;
    while (std::getline(stream, line) && line != "\r") {
        if (line.find("Content-Type:") == 0) {
            request.content_type = line.substr(14);
            // 移除可能的\r
            if (!request.content_type.empty() && request.content_type.back() == '\r') {
                request.content_type.pop_back();
            }
        } else if (line.find("Content-Length:") == 0) {
            content_length = std::stoull(line.substr(16));
        }
    }
    
    // 解析请求体
    if (content_length > 0) {
        std::string body;
        char ch;
        while (body.size() < content_length && stream.get(ch)) {
            body += ch;
        }
        request.body = body;
    }
    
    return request;
}

std::string HttpServer::buildResponse(const HttpResponse& response) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << response.status_code << " " << response.status_text << "\r\n";
    oss << "Content-Type: " << response.content_type << "\r\n";
    oss << "Content-Length: " << response.body.size() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "\r\n";
    oss << response.body;
    return oss.str();
}

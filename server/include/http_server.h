#pragma once

#include <string>
#include <functional>
#include <memory>
#include <map>
#include <netinet/in.h>

/**
 * 简单的HTTP请求结构
 */
struct HttpRequest {
    std::string method;        // GET, POST等
    std::string path;          // 请求路径
    std::string body;          // 请求体
    std::string content_type;  // Content-Type
};

/**
 * 简单的HTTP响应结构
 */
struct HttpResponse {
    int status_code = 200;
    std::string status_text = "OK";
    std::string body;
    std::string content_type = "application/json";
};

/**
 * HTTP请求处理器回调函数类型
 */
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

/**
 * 简单的HTTP服务器实现
 */
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
    int port_;
    int server_fd_;
    bool running_;
    
    // 处理客户端连接
    void handleClient(int client_fd);
    
    // 解析HTTP请求
    HttpRequest parseRequest(const std::string& raw_request);
    
    // 构建HTTP响应
    std::string buildResponse(const HttpResponse& response);
    
    // 路由表
    std::map<std::string, HttpHandler> handlers_;
};

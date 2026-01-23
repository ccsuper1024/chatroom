#include "http/http_server.h"
#include "net/acceptor.h"
#include "logger.h"
#include "utils/thread_pool.h"
#include "http/http_codec.h"
#include "net/tcp_connection.h"
#include "utils/server_config.h"
#include "net/event_loop_thread_pool.h"
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
#include <filesystem>
#include <cctype>

struct HttpConnectionContext {
    enum Protocol { kHttp, kWebSocket };
    Protocol protocol = kHttp;
};

HttpServer::HttpServer(EventLoop* loop, int port) 
    : server_(loop, InetAddress(port), "HttpServer", TcpServer::kReusePort),
      port_(port),
      thread_pool_(ServerConfig::instance().thread_pool.core_threads,
                   ServerConfig::instance().thread_pool.max_threads,
                   ServerConfig::instance().thread_pool.queue_capacity) {
    
    // Set IO threads
    int ioThreads = ServerConfig::instance().thread_pool.io_threads;
    if (ioThreads > 0) {
        server_.setThreadNum(ioThreads);
    }

    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::registerHandler(const std::string& path, HttpHandler handler) {
    handlers_[path] = handler;
    LOG_INFO("注册路由: {}", path);
}

void HttpServer::setWebSocketHandler(WebSocketHandler handler) {
    ws_handler_ = std::move(handler);
}

void HttpServer::setStaticResourceDir(const std::string& dir) {
    static_resource_dir_ = dir;
}

void HttpServer::start() {
    server_.start();
    LOG_INFO("HTTP服务器启动，监听端口: {}", server_.ipPort());
}

void HttpServer::stop() {
    // TcpServer manages its own lifecycle
    LOG_INFO("HTTP服务器已停止");
}

std::size_t HttpServer::getThreadPoolQueueSize() const {
    return thread_pool_.queueSize();
}

std::size_t HttpServer::getThreadPoolRejectedCount() const {
    return thread_pool_.rejectedCount();
}

std::size_t HttpServer::getThreadPoolThreadCount() const {
    return thread_pool_.currentThreadCount();
}

std::size_t HttpServer::getThreadPoolActiveThreadCount() const {
    return thread_pool_.activeThreadCount();
}

void HttpServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        conn->setContext(HttpConnectionContext());
    }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime) {
    (void)receiveTime;
    // printf("HttpServer::onMessage\n");
    if (!conn->getContext().has_value()) {
        conn->setContext(HttpConnectionContext());
    }

    HttpConnectionContext* context = std::any_cast<HttpConnectionContext>(conn->getMutableContext());
    // printf("HttpServer::onMessage protocol=%d\n", context->protocol);

    while (buf->readableBytes() > 0) {
        if (context->protocol == HttpConnectionContext::kHttp) {
            LOG_INFO("处理HTTP请求");
            bool complete = false;
            bool bad = false;
            HttpRequest req = parseRequestFromBuffer(buf, complete, bad);
            
            if (bad) {
                conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
                conn->forceClose();
                return;
            }
            
            if (complete) {
                onRequest(conn, req);
                // Protocol might have changed to WebSocket in onRequest
                continue;
            } else {
                LOG_INFO("未完成请求，等待更多数据");
                break; // Wait for more data
            }
        } else if (context->protocol == HttpConnectionContext::kWebSocket) {
            LOG_INFO("处理WebSocket请求");
            protocols::WebSocketFrame frame;
            // Use beginRead() to get mutable pointer for in-place unmasking
            int consumed = protocols::WebSocketCodec::parseFrame(reinterpret_cast<uint8_t*>(buf->beginRead()), buf->readableBytes(), frame);
            
            if (consumed > 0) {
                buf->retrieve(consumed);
                if (ws_handler_) {
                    ws_handler_(conn, frame);
                }
            } else if (consumed < 0) {
                conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
                conn->forceClose();
                return;
            } else {
                break; // Incomplete frame
            }
        }else{
            LOG_ERROR("未知协议");
            conn->forceClose();
            return;
        }
    }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req) {
    // Check for WebSocket Upgrade
    if (req.headers.count("Upgrade") && req.headers.at("Upgrade") == "websocket") {
        HttpConnectionContext* context = std::any_cast<HttpConnectionContext>(conn->getMutableContext());
        context->protocol = HttpConnectionContext::kWebSocket;
        
        std::string secKey = "";
        if (req.headers.count("Sec-WebSocket-Key")) {
            secKey = req.headers.at("Sec-WebSocket-Key");
        }
        LOG_INFO("WebSocket连接升级请求，Sec-WebSocket-Key: {}", secKey);
        
        std::string acceptKey = protocols::WebSocketCodec::computeAcceptKey(secKey);
        LOG_INFO("WebSocket连接升级响应，Sec-WebSocket-Accept: {}", acceptKey);
        
        std::string resp = "HTTP/1.1 101 Switching Protocols\r\n"
                           "Upgrade: websocket\r\n"
                           "Connection: Upgrade\r\n"
                           "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
        conn->send(resp);
        return;
    }

    std::string handler_path = req.path;
    auto query_pos = handler_path.find('?');
    if (query_pos != std::string::npos) {
        handler_path = handler_path.substr(0, query_pos);
    }

    if (handlers_.count(handler_path)) {
        // Dispatch to thread pool
        thread_pool_.post([this, conn, req, handler = handlers_[handler_path]]() {
            HttpResponse resp = handler(req);
            
            // Send response back in IO loop
            conn->getLoop()->runInLoop([this, conn, resp]() {
                std::string responseStr = buildResponse(resp);
                conn->send(responseStr);
            });
        });
    } else {
        // Try to serve static file
        if (!static_resource_dir_.empty() && (req.method == "GET" || req.method == "HEAD")) {
            thread_pool_.post([this, conn, req]() {
                std::string url_path = req.path;
                // Default to index.html for root
                if (url_path == "/") {
                    url_path = "/index.html";
                }
                
                HttpResponse resp = serveStaticFile(url_path);
                
                // Handle HEAD method: set Content-Length and clear body
                if (req.method == "HEAD") {
                    resp.headers["Content-Length"] = std::to_string(resp.body.size());
                    resp.body.clear();
                }

                conn->getLoop()->runInLoop([conn, resp, this]() {
                    std::string responseStr = buildResponse(resp);
                    conn->send(responseStr);
                });
            });
            return;
        }

        HttpResponse resp;
        resp.status_code = 404;
        resp.status_text = "Not Found";
        conn->send(buildResponse(resp));
    }
}

HttpResponse HttpServer::serveStaticFile(const std::string& path) {
    HttpResponse resp;
    try {
        // Sanitize path to prevent directory traversal
        if (path.find("..") != std::string::npos) {
            resp.status_code = 403;
            resp.status_text = "Forbidden";
            return resp;
        }

        std::filesystem::path resource_path(static_resource_dir_);
        // Remove leading slash
        std::string relative_path = path.size() > 0 && path[0] == '/' ? path.substr(1) : path;
        std::filesystem::path file_path = resource_path / relative_path;
        LOG_INFO("Serving static file: {}", std::filesystem::absolute(file_path).string());

        if (std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path)) {
            std::ifstream file(file_path, std::ios::binary);
            if (file) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                resp.body = buffer.str();
                resp.status_code = 200;
                resp.status_text = "OK";
                
                // Determine Content-Type
                std::string ext = file_path.extension().string();
                if (ext == ".html") resp.content_type = "text/html";
                else if (ext == ".css") resp.content_type = "text/css";
                else if (ext == ".js") resp.content_type = "application/javascript";
                else if (ext == ".png") resp.content_type = "image/png";
                else if (ext == ".jpg" || ext == ".jpeg") resp.content_type = "image/jpeg";
                else if (ext == ".svg") resp.content_type = "image/svg+xml";
                else if (ext == ".ico") resp.content_type = "image/x-icon";
                else resp.content_type = "application/octet-stream";
                
                return resp;
            }
        } else {
             // Debug log for 404
             LOG_WARN("Static file not found: {} (Full path: {})", path, std::filesystem::absolute(file_path).string());
        }
        
        resp.status_code = 404;
        resp.status_text = "Not Found";
    } catch (const std::exception& e) {
        LOG_ERROR("Serve static file error: {}", e.what());
        resp.status_code = 500;
        resp.status_text = "Internal Server Error";
    }
    return resp;
}

std::string HttpServer::buildResponse(const HttpResponse& resp) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << resp.status_code << " " << resp.status_text << "\r\n";
    oss << "Content-Type: " << resp.content_type << "\r\n";
    if (resp.headers.find("Content-Length") == resp.headers.end()) {
        oss << "Content-Length: " << resp.body.size() << "\r\n";
    }
    for (const auto& [key, value] : resp.headers) {
        oss << key << ": " << value << "\r\n";
    }
    oss << "Connection: keep-alive\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "\r\n";
    oss << resp.body;
    return oss.str();
}

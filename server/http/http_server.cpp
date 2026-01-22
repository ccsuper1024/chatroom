#include "http/http_server.h"
#include "net/acceptor.h"
#include "logger.h"
#include "utils/thread_pool.h"
#include "http/http_codec.h"
#include "net/tcp_connection.h"
#include "utils/server_config.h"
#include "net/event_loop_thread_pool.h"
#include "utils/server_error.h"
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
#include <string_view>

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
    ws_handler_ = handler;
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
    // printf("HttpServer::onMessage\n");
    if (!conn->getContext().has_value()) {
        conn->setContext(HttpConnectionContext());
    }

    HttpConnectionContext* context = std::any_cast<HttpConnectionContext>(conn->getMutableContext());
    // printf("HttpServer::onMessage protocol=%d\n", context->protocol);

    while (buf->readableBytes() > 0) {
        if (context->protocol == HttpConnectionContext::kHttp) {
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
                break; // Wait for more data
            }
        } else if (context->protocol == HttpConnectionContext::kWebSocket) {
            protocols::WebSocketFrame frame;
            // Use beginRead() to get mutable pointer for in-place unmasking
            int consumed = protocols::WebSocketCodec::parseFrame(reinterpret_cast<uint8_t*>(buf->beginRead()), buf->readableBytes(), frame);
            
            if (consumed > 0) {
                buf->retrieve(consumed);
                if (ws_handler_) {
                    ws_handler_(conn, frame);
                }
            } else if (consumed < 0) {
                conn->forceClose();
                return;
            } else {
                break; // Incomplete frame
            }
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
        std::string acceptKey = protocols::WebSocketCodec::computeAcceptKey(secKey);
        
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
        thread_pool_.post([conn, req, handler = handlers_[handler_path]]() {
            HttpResponse resp = handler(req);
            
            // Send response back in IO loop
            conn->getLoop()->runInLoop([conn, resp]() {
                std::string responseStr = buildResponse(resp);
                conn->send(responseStr);
            });
        });
    } else {
        HttpResponse resp;
        resp.status_code = 404;
        resp.status_text = "Not Found";
        conn->send(buildResponse(resp));
    }
}

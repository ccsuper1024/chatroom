#include "rtsp/rtsp_server.h"
#include "net/tcp_connection.h"
#include "logger.h"

RtspServer::RtspServer(EventLoop* loop, int port)
    : server_(loop, InetAddress(port), "RtspServer"), port_(port) {
    server_.setConnectionCallback(
        std::bind(&RtspServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&RtspServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

RtspServer::~RtspServer() {
}

void RtspServer::setRtspHandler(RtspHandler handler) {
    rtsp_handler_ = std::move(handler);
}

void RtspServer::start() {
    server_.start();
    LOG_INFO("RTSP Server started on port {}", server_.ipPort());
}

void RtspServer::onConnection(const std::shared_ptr<TcpConnection>& conn) {
    if (conn->connected()) {
        LOG_INFO("RTSP Connection established from {}", conn->peerAddress().toIpPort());
    } else {
        LOG_INFO("RTSP Connection closed from {}", conn->peerAddress().toIpPort());
    }
}

void RtspServer::onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buf, Timestamp time) {
    (void)time;
    while (buf->readableBytes() > 0) {
        protocols::RtspRequest request;
        size_t consumed = protocols::RtspCodec::parseRequest(buf, request);
        
        if (consumed > 0) {
            buf->retrieve(consumed);
            if (rtsp_handler_) {
                rtsp_handler_(conn, request);
            }
        } else {
            break; 
        }
    }
}

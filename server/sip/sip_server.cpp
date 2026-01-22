#include "sip/sip_server.h"
#include "net/tcp_connection.h"
#include "logger.h"

SipServer::SipServer(EventLoop* loop, int port)
    : server_(loop, InetAddress(port), "SipServer"), port_(port) {
    server_.setConnectionCallback(
        std::bind(&SipServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&SipServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

SipServer::~SipServer() {
}

void SipServer::setSipHandler(SipHandler handler) {
    sip_handler_ = std::move(handler);
}

void SipServer::start() {
    server_.start();
    LOG_INFO("SIP Server started on port {}", server_.ipPort());
}

void SipServer::onConnection(const std::shared_ptr<TcpConnection>& conn) {
    if (conn->connected()) {
        LOG_INFO("SIP Connection established from {}", conn->peerAddress().toIpPort());
    } else {
        LOG_INFO("SIP Connection closed from {}", conn->peerAddress().toIpPort());
    }
}

void SipServer::onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buf, Timestamp time) {
    (void)time;
    while (buf->readableBytes() > 0) {
        std::string data(buf->peek(), buf->readableBytes());
        SipRequest request;
        size_t consumed = SipCodec::parseRequest(data, request);
        
        if (consumed > 0) {
            std::string raw_msg(buf->peek(), consumed);
            buf->retrieve(consumed);
            if (sip_handler_) {
                sip_handler_(conn, request, raw_msg);
            }
        } else {
            break;
        }
    }
}

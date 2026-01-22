#include "ftp/ftp_server.h"
#include "net/tcp_connection.h"
#include "logger.h"

FtpServer::FtpServer(EventLoop* loop, int port)
    : server_(loop, InetAddress(port), "FtpServer"), port_(port) {
    server_.setConnectionCallback(
        std::bind(&FtpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&FtpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

FtpServer::~FtpServer() {
}

void FtpServer::setFtpHandler(FtpHandler handler) {
    ftp_handler_ = std::move(handler);
}

void FtpServer::start() {
    server_.start();
    LOG_INFO("FTP Server started on port {}", server_.ipPort());
}

void FtpServer::onConnection(const std::shared_ptr<TcpConnection>& conn) {
    if (conn->connected()) {
        LOG_INFO("FTP Connection established from {}", conn->peerAddress().toIpPort());
        conn->send("220 Service ready for new user.\r\n");
    } else {
        LOG_INFO("FTP Connection closed from {}", conn->peerAddress().toIpPort());
    }
}

void FtpServer::onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buf, Timestamp time) {
    (void)time;
    while (buf->readableBytes() > 0) {
        const char* crlf = buf->findCRLF();
        if (crlf) {
            std::string command(buf->peek(), crlf - buf->peek());
            buf->retrieve(crlf + 2 - buf->peek());
            if (ftp_handler_) {
                ftp_handler_(conn, command);
            }
        } else {
            break;
        }
    }
}

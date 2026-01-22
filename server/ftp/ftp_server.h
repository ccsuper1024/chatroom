#pragma once

#include <functional>
#include <memory>
#include "net/tcp_server.h"
#include "net/event_loop.h"

class TcpConnection;

using FtpHandler = std::function<void(const std::shared_ptr<TcpConnection>&, const std::string&)>;

class FtpServer {
public:
    FtpServer(EventLoop* loop, int port);
    ~FtpServer();

    void setFtpHandler(FtpHandler handler);
    void start();
    
    /**
     * @brief 获取监听端口
     */
    int port() const { return port_; }

private:
    void onConnection(const std::shared_ptr<TcpConnection>& conn);
    void onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buf, Timestamp time);

    TcpServer server_;
    int port_;
    FtpHandler ftp_handler_;
};

#pragma once

#include <memory>
#include <string>

class HttpServer;
class EventLoop;
class Channel;

class TcpConnection {
public:
    TcpConnection(HttpServer* server, EventLoop* loop, int fd, const std::string& ip);
    ~TcpConnection();

    int fd() const;
    std::string ip() const { return ip_; }
    bool closed() const;

    void handleRead();
    void handleWrite();
    void handleClose();

    void appendResponse(const std::string& data);
    void shutdown();
    void setCloseAfterWrite(bool close) { close_after_write_ = close; }

private:
    HttpServer* server_;
    EventLoop* loop_;
    int fd_;
    std::string ip_;
    bool closed_;
    bool close_after_write_ = false;
    std::string read_buffer_;
    std::string write_buffer_;
    std::unique_ptr<Channel> channel_;
};


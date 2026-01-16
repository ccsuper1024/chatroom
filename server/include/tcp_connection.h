#pragma once

#include <memory>
#include <string>

class HttpServer;
class EventLoop;
class Channel;

class TcpConnection {
public:
    TcpConnection(HttpServer* server, EventLoop* loop, int fd);
    ~TcpConnection();

    int fd() const;
    bool closed() const;

    void handleRead();
    void handleWrite();
    void handleClose();

    void appendResponse(const std::string& data);
    void shutdown();

private:
    HttpServer* server_;
    EventLoop* loop_;
    int fd_;
    bool closed_;
    std::string read_buffer_;
    std::string write_buffer_;
    std::unique_ptr<Channel> channel_;
};


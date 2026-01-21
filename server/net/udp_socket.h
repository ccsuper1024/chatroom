#pragma once

#include "net/channel.h"
#include <functional>
#include <memory>
#include <string>
#include <netinet/in.h>

class EventLoop;

class UdpSocket {
public:
    using MessageCallback = std::function<void(const char* data, size_t len, const struct sockaddr_in& addr)>;

    UdpSocket(EventLoop* loop, int port);
    ~UdpSocket();

    bool bind();
    void sendTo(const char* data, size_t len, const struct sockaddr_in& addr);
    void sendTo(const std::string& data, const struct sockaddr_in& addr);
    
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }

    int fd() const { return fd_; }

private:
    void handleRead();

    EventLoop* loop_;
    int port_;
    int fd_;
    std::unique_ptr<Channel> channel_;
    MessageCallback messageCallback_;
};

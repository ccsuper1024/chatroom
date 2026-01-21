#pragma once

#include <functional>
#include <string>
#include "net/channel.h"
#include "net/inet_address.h"

class EventLoop;

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb) {
        newConnectionCallback_ = cb;
    }

    void listen();
    bool listening() const { return listening_; }

private:
    void handleRead();

    EventLoop* loop_;
    int acceptSocketFd_; // Replaces listen_fd_
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
    int idleFd_; // For EMFILE handling
};


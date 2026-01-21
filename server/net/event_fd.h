#pragma once

#include "net/channel.h"
#include <functional>
#include <memory>

class EventLoop;

class EventFd {
public:
    using EventCallback = std::function<void()>;

    explicit EventFd(EventLoop* loop);
    ~EventFd();

    void notify();
    void setCallback(EventCallback cb) { callback_ = std::move(cb); }

private:
    void handleRead();

    EventLoop* loop_;
    int fd_;
    std::unique_ptr<Channel> channel_;
    EventCallback callback_;
};

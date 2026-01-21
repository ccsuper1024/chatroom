#pragma once

#include "net/channel.h"
#include <functional>
#include <memory>
#include <vector>
#include <sys/signalfd.h>
#include <signal.h>

class EventLoop;

class SignalFd {
public:
    using SignalCallback = std::function<void(int)>;

    explicit SignalFd(EventLoop* loop);
    ~SignalFd();

    void addSignal(int signo);
    void setCallback(SignalCallback cb) { callback_ = std::move(cb); }

private:
    void handleRead();

    EventLoop* loop_;
    int fd_;
    std::unique_ptr<Channel> channel_;
    SignalCallback callback_;
    sigset_t mask_;
};

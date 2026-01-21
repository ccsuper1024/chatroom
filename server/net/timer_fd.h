#pragma once

#include "net/channel.h"
#include <functional>
#include <memory>
#include <chrono>

class EventLoop;

class TimerFd {
public:
    using TimerCallback = std::function<void()>;

    TimerFd(EventLoop* loop);
    ~TimerFd();

    // Start the timer. If interval > 0, it's periodic.
    // initial_delay: milliseconds
    // interval: milliseconds (0 for one-shot)
    void start(int initial_delay_ms, int interval_ms = 0);
    
    void stop();

    void setCallback(TimerCallback cb) { callback_ = std::move(cb); }

private:
    void handleRead();

    EventLoop* loop_;
    int fd_;
    std::unique_ptr<Channel> channel_;
    TimerCallback callback_;
    bool running_;
};

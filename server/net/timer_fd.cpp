#include "net/timer_fd.h"
#include "net/event_loop.h"
#include "logger.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include <cstring>

TimerFd::TimerFd(EventLoop* loop)
    : loop_(loop), fd_(::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)), running_(false) {
    if (fd_ < 0) {
        LOG_ERROR("Failed to create timerfd");
    }
    channel_ = std::make_unique<Channel>(loop, fd_);
    channel_->setReadCallback([this]() { handleRead(); });
}

TimerFd::~TimerFd() {
    stop();
    channel_->disableAll();
    channel_->remove();
    ::close(fd_);
}

void TimerFd::start(int initial_delay_ms, int interval_ms) {
    if (fd_ < 0) return;

    struct itimerspec newValue;
    std::memset(&newValue, 0, sizeof(newValue));

    // Initial expiration
    newValue.it_value.tv_sec = initial_delay_ms / 1000;
    newValue.it_value.tv_nsec = (initial_delay_ms % 1000) * 1000000;

    // Interval
    if (interval_ms > 0) {
        newValue.it_interval.tv_sec = interval_ms / 1000;
        newValue.it_interval.tv_nsec = (interval_ms % 1000) * 1000000;
    }

    int ret = ::timerfd_settime(fd_, 0, &newValue, nullptr);
    if (ret) {
        LOG_ERROR("timerfd_settime failed");
        return;
    }

    channel_->enableReading();
    running_ = true;
}

void TimerFd::stop() {
    if (!running_) return;
    
    struct itimerspec newValue;
    std::memset(&newValue, 0, sizeof(newValue));
    ::timerfd_settime(fd_, 0, &newValue, nullptr);
    
    channel_->disableAll(); // Don't remove, just disable
    running_ = false;
}

void TimerFd::handleRead() {
    uint64_t howmany;
    ssize_t n = ::read(fd_, &howmany, sizeof(howmany));
    if (n != sizeof(howmany)) {
        LOG_ERROR("TimerFd::handleRead reads {} bytes instead of 8", n);
    }
    if (callback_) {
        callback_();
    }
}

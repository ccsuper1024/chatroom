#include "net/event_fd.h"
#include "net/event_loop.h"
#include "logger.h"
#include <sys/eventfd.h>
#include <unistd.h>

EventFd::EventFd(EventLoop* loop)
    : loop_(loop), fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
    if (fd_ < 0) {
        LOG_ERROR("Failed to create eventfd");
    }
    channel_ = std::make_unique<Channel>(loop, fd_);
    channel_->setReadCallback([this]() { handleRead(); });
    channel_->enableReading();
}

EventFd::~EventFd() {
    channel_->disableAll();
    channel_->remove();
    ::close(fd_);
}

void EventFd::notify() {
    uint64_t one = 1;
    ssize_t n = ::write(fd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventFd::notify writes {} bytes instead of 8", n);
    }
}

void EventFd::handleRead() {
    uint64_t one = 1;
    ssize_t n = ::read(fd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR("EventFd::handleRead reads {} bytes instead of 8", n);
    }
    if (callback_) {
        callback_();
    }
}

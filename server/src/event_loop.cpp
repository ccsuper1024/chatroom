#include "event_loop.h"
#include "channel.h"

#include <unistd.h>
#include <sys/eventfd.h>
#include <iostream>

EventLoop::EventLoop()
    : epoll_fd_(::epoll_create1(0)),
      wakeup_fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)),
      running_(false),
      events_(64) {
    if (wakeup_fd_ < 0) {
        std::cerr << "Failed to create eventfd" << std::endl;
        abort();
    }
    wakeup_channel_ = std::make_unique<Channel>(this, wakeup_fd_);
    wakeup_channel_->setReadCallback([this]() { handleWakeup(); });
    wakeup_channel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeup_channel_->disableAll();
    wakeup_channel_->remove();
    ::close(wakeup_fd_);
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeup_fd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        std::cerr << "EventLoop::wakeup() writes " << n << " bytes instead of 8" << std::endl;
    }
}

void EventLoop::handleWakeup() {
    uint64_t one = 1;
    ssize_t n = ::read(wakeup_fd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        std::cerr << "EventLoop::handleWakeup() reads " << n << " bytes instead of 8" << std::endl;
    }
}

void EventLoop::loop(const std::function<void()>& postLoopHook) {
    running_ = true;
    while (running_) {
        int n = ::epoll_wait(epoll_fd_, events_.data(),
                             static_cast<int>(events_.size()), 1000);
        if (n <= 0) {
            if (postLoopHook) {
                postLoopHook();
            }
            continue;
        }

        if (static_cast<size_t>(n) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
        
        for (int i = 0; i < n; ++i) {
            auto* ch = static_cast<Channel*>(events_[i].data.ptr);
            if (ch) {
                ch->handleEvent(events_[i].events);
            }
        }
        if (postLoopHook) {
            postLoopHook();
        }
    }
}

void EventLoop::stop() {
    running_ = false;
}

void EventLoop::addChannel(Channel* channel) {
    epoll_event ev;
    ev.events = channel->events();
    ev.data.ptr = channel;
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, channel->fd(), &ev);
}

void EventLoop::updateChannel(Channel* channel) {
    epoll_event ev;
    ev.events = channel->events();
    ev.data.ptr = channel;
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, channel->fd(), &ev);
}

void EventLoop::removeChannel(Channel* channel) {
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, channel->fd(), nullptr);
}

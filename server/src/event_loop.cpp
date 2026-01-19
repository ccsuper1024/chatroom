#include "event_loop.h"
#include "channel.h"

#include <unistd.h>

EventLoop::EventLoop()
    : epoll_fd_(::epoll_create1(0)),
      running_(false),
      events_(64) {
}

EventLoop::~EventLoop() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
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

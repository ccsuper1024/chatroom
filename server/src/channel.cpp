#include "channel.h"
#include "event_loop.h"

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0) {
}

int Channel::fd() const {
    return fd_;
}

uint32_t Channel::events() const {
    return events_;
}

void Channel::setEvents(uint32_t events) {
    events_ = events;
}

void Channel::setReadCallback(Callback cb) {
    readCallback_ = std::move(cb);
}

void Channel::setWriteCallback(Callback cb) {
    writeCallback_ = std::move(cb);
}

void Channel::setCloseCallback(Callback cb) {
    closeCallback_ = std::move(cb);
}

void Channel::handleEvent(uint32_t revents) {
    if (revents & (EPOLLHUP | EPOLLERR)) {
        if (closeCallback_) {
            closeCallback_();
        }
        return;
    }
    if ((revents & EPOLLIN) && readCallback_) {
        readCallback_();
    }
    if ((revents & EPOLLOUT) && writeCallback_) {
        writeCallback_();
    }
}

EventLoop* Channel::ownerLoop() const {
    return loop_;
}

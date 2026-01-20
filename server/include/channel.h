#pragma once

#include <cstdint>
#include <functional>

class EventLoop;

class Channel {
public:
    using Callback = std::function<void()>;

    Channel(EventLoop* loop, int fd);

    int fd() const;
    uint32_t events() const;
    void setEvents(uint32_t events);

    void setReadCallback(Callback cb);
    void setWriteCallback(Callback cb);
    void setCloseCallback(Callback cb);

    void enableReading();
    void disableAll();
    void remove();

    void handleEvent(uint32_t revents);

    EventLoop* ownerLoop() const;

private:
    EventLoop* loop_;
    int fd_;
    uint32_t events_;
    Callback readCallback_;
    Callback writeCallback_;
    Callback closeCallback_;
};

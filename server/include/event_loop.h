#pragma once

#include <vector>
#include <sys/epoll.h>
#include <cstdint>
#include <functional>
#include <memory>

class Channel;

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void loop(const std::function<void()>& postLoopHook);
    void stop();

    void addChannel(Channel* channel);
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

    void wakeup();

private:
    void handleWakeup();

    int epoll_fd_;
    int wakeup_fd_;
    std::unique_ptr<Channel> wakeup_channel_;
    bool running_;
    std::vector<epoll_event> events_;
};

#pragma once

#include <vector>
#include <sys/epoll.h>
#include <cstdint>
#include <functional>

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

private:
    int epoll_fd_;
    bool running_;
    std::vector<epoll_event> events_;
};

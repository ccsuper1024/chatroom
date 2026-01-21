#pragma once

#include <vector>
#include <sys/epoll.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

class Channel;

class EventLoop {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void stop();

    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);

    void addChannel(Channel* channel);
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

    void wakeup();
    
    bool isInLoopThread() const { return thread_id_ == std::this_thread::get_id(); }

private:
    void handleWakeup();
    void doPendingFunctors();

    int epoll_fd_;
    int wakeup_fd_;
    std::unique_ptr<Channel> wakeup_channel_;
    bool running_;
    std::vector<epoll_event> events_;
    
    std::thread::id thread_id_;
    std::mutex mutex_;
    std::vector<Functor> pending_functors_;
    bool calling_pending_functors_;
};

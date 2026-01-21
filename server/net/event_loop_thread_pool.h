#pragma once

#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool {
public:
    EventLoopThreadPool(EventLoop* baseLoop);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads);
    void start();

    EventLoop* getNextLoop();

private:
    EventLoop* baseLoop_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;
};

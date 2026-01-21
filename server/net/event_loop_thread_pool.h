#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <string>

#include "net/event_loop_thread.h"

class EventLoop;

class EventLoopThreadPool {
public:
    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();
    
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    void start(const std::function<void(EventLoop*)>& cb = std::function<void(EventLoop*)>());
    
    EventLoop* getNextLoop();
    
    bool started() const { return started_; }
    const std::string& name() const { return name_; }

private:
    EventLoop* baseLoop_;
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<EventLoop*> loops_;
    // std::vector<std::unique_ptr<EventLoopThread>> threads_; // Pending implementation
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
};

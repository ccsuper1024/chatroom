#include "net/event_loop_thread_pool.h"
#include "net/event_loop_thread.h"
#include "net/event_loop.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop)
    : baseLoop_(baseLoop),
      started_(false),
      numThreads_(0),
      next_(0) {
}

EventLoopThreadPool::~EventLoopThreadPool() {
    // Unique pointers clean up threads
}

void EventLoopThreadPool::setThreadNum(int numThreads) {
    numThreads_ = numThreads;
}

void EventLoopThreadPool::start() {
    started_ = true;

    for (int i = 0; i < numThreads_; ++i) {
        auto t = std::make_unique<EventLoopThread>();
        loops_.push_back(t->startLoop());
        threads_.push_back(std::move(t));
    }

    if (numThreads_ == 0 && baseLoop_) {
        loops_.push_back(baseLoop_);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    EventLoop* loop = baseLoop_;

    if (!loops_.empty()) {
        loop = loops_[next_];
        next_ = (next_ + 1) % loops_.size();
    }
    return loop;
}

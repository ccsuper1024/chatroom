#include "event_loop_thread.h"
#include "event_loop.h"

EventLoopThread::EventLoopThread()
    : loop_(nullptr),
      exiting_(false) {
}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_) {
        loop_->stop();
        loop_->wakeup();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop() {
    thread_ = std::thread(&EventLoopThread::threadFunc, this);

    std::unique_lock<std::mutex> lock(mutex_);
    while (loop_ == nullptr) {
        cond_.wait(lock);
    }
    return loop_;
}

void EventLoopThread::threadFunc() {
    EventLoop loop;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();
    // Loop ends
    std::lock_guard<std::mutex> lock(mutex_);
    loop_ = nullptr;
}

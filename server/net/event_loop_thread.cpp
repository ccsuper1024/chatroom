#include "net/event_loop_thread.h"
#include "net/event_loop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                               const std::string& name)
    : loop_(nullptr),
      exiting_(false),
      callback_(cb),
      name_(name) {
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
    
    if (callback_) {
        callback_(&loop);
    }

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

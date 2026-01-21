#pragma once

#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include "net/callbacks.h"
#include "net/timestamp.h"

class Channel;
class Poller;

namespace net {
class TimerQueue;
}

/**
 * @brief EventLoop 事件循环类
 * 
 * Reactor 模式的核心。
 * 每个线程只能有一个 EventLoop (One Loop Per Thread)。
 * 负责循环获取 Poller 就绪的事件，并分发给 Channel 处理。
 */
class EventLoop {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    /**
     * @brief 开启事件循环
     * 
     * 必须在创建对象的线程中调用。
     */
    void loop();

    /**
     * @brief 停止事件循环
     * 
     * 可以跨线程调用。
     */
    void stop(); // formerly stop(), usually quit()

    /**
     * @brief 在 Loop 线程中执行回调
     * 
     * 如果当前是 Loop 线程，直接执行；否则加入队列并唤醒。
     */
    void runInLoop(Functor cb);

    /**
     * @brief 将回调加入队列
     * 
     * 加入队列并在必要时唤醒 Loop 线程。
     */
    void queueInLoop(Functor cb);

    // Channel 管理
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 唤醒 Loop
    void wakeup();

    // 定时器接口
    void runAt(Timestamp time, TimerCallback cb);
    void runAfter(double delay, TimerCallback cb);
    void runEvery(double interval, TimerCallback cb);

    bool isInLoopThread() const { return threadId_ == std::this_thread::get_id(); }
    void assertInLoopThread() {
        if (!isInLoopThread()) {
            // abort or log
        }
    }
    
    static EventLoop* getEventLoopOfCurrentThread();

private:
    void handleRead(); // handle wakeup
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    bool looping_;
    std::atomic<bool> quit_; // atomic for stop()
    bool eventHandling_;
    bool callingPendingFunctors_;
    
    const std::thread::id threadId_;
    
    std::unique_ptr<Poller> poller_;
    
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;
    
    ChannelList activeChannels_;
    
    std::mutex mutex_;
    std::vector<Functor> pendingFunctors_;
    
    std::unique_ptr<net::TimerQueue> timerQueue_;
};

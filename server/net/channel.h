#pragma once

#include <functional>
#include <memory>
#include <sys/epoll.h>

class EventLoop;

/**
 * @brief Channel 类
 * 
 * 封装了文件描述符(fd)及其感兴趣的事件(events)和实际发生的事件(revents)。
 * 负责绑定回调函数，并在事件发生时执行相应的回调。
 * 它是 Reactor 模式中 Event 的抽象。
 */
class Channel {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    /**
     * @brief 处理事件
     * 
     * 由 EventLoop 调用，根据 revents_ 分发到具体的回调函数。
     */
    void handleEvent();

    /**
     * @brief 绑定对象的生命周期
     * 
     * 防止 Channel 对应的对象（如 TcpConnection）在回调执行前被销毁。
     */
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    int events() const { return events_; }
    void setRevents(int revt) { revents_ = revt; }
    int index() const { return index_; }
    void set_index(int idx) { index_ = idx; }

    // 回调注册
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 事件控制
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }
    void enableET() { events_ |= EPOLLET; update(); }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    EventLoop* ownerLoop() const { return loop_; }
    void remove();

private:
    void update();
    void handleEventWithGuard();

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;
    const int fd_;
    int events_;
    int revents_;
    int index_; // used by Poller
    bool tied_;
    std::weak_ptr<void> tie_;

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

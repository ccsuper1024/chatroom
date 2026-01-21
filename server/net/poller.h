#pragma once

#include "net/channel.h"
#include <vector>
#include <unordered_map>
#include <chrono>

class EventLoop;

/**
 * @brief Poller 抽象基类
 * 
 * 定义了 I/O 多路复用的统一接口。
 * 具体实现可以是 EpollPoller (Linux) 或 PollPoller (其他).
 */
class Poller {
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller();

    /**
     * @brief 等待 I/O 事件
     * 
     * @param timeoutMs 超时时间(毫秒)
     * @param activeChannels 输出参数，返回活跃的 Channel 列表
     * @return std::chrono::system_clock::time_point 返回事件发生的时间
     */
    virtual std::chrono::system_clock::time_point poll(int timeoutMs, ChannelList* activeChannels) = 0;

    /**
     * @brief 更新 Channel 感兴趣的事件
     * 
     * 对应 epoll_ctl 的 MOD/ADD 操作
     */
    virtual void updateChannel(Channel* channel) = 0;

    /**
     * @brief 移除 Channel
     * 
     * 对应 epoll_ctl 的 DEL 操作
     */
    virtual void removeChannel(Channel* channel) = 0;

    /**
     * @brief 判断 Channel 是否在当前 Poller 中
     */
    virtual bool hasChannel(Channel* channel) const;

    /**
     * @brief 获取默认的 Poller 实现
     */
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    EventLoop* ownerLoop_;
};

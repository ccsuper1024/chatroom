#pragma once

#include "net/poller.h"
#include <vector>
#include <sys/epoll.h>

/**
 * @brief EpollPoller 类
 * 
 * 基于 epoll 的 I/O 复用实现。
 */
class EpollPoller : public Poller {
public:
    EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    std::chrono::system_clock::time_point poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16;

    /**
     * @brief 填充活跃的 Channel
     */
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    
    /**
     * @brief 更新 epoll 关注的事件
     */
    void update(int operation, Channel* channel);

    int epollfd_;
    std::vector<struct epoll_event> events_;
};

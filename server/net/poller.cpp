#include "net/poller.h"
#include "net/channel.h"
#include "net/poller/epoll_poller.h"
#include <cstdlib>

Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop) {
}

Poller::~Poller() = default;

bool Poller::hasChannel(Channel* channel) const {
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}

Poller* Poller::newDefaultPoller(EventLoop* loop) {
    if (::getenv("CHATROOM_USE_POLL")) {
        return nullptr; // PollPoller not implemented
    } else {
        return new EpollPoller(loop);
    }
}

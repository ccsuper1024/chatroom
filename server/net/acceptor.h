#pragma once

#include <functional>
#include <string>

class EventLoop;
class Channel;

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int, const std::string&)>;

    Acceptor(EventLoop* loop, int port);
    ~Acceptor();

    void setNewConnectionCallback(NewConnectionCallback cb);

    bool isValid() const;

private:
    void handleRead();

    EventLoop* loop_;
    int listen_fd_;
    int port_;
    Channel* channel_;
    NewConnectionCallback callback_;
};

#include "acceptor.h"
#include "event_loop.h"
#include "channel.h"
#include "logger.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <arpa/inet.h>

Acceptor::Acceptor(EventLoop* loop, int port)
    : loop_(loop),
      listen_fd_(-1),
      port_(port),
      channel_(nullptr) {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        LOG_ERROR("创建监听套接字失败");
        return;
    }

    int opt = 1;
    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_ERROR("设置监听套接字选项失败");
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("绑定端口 {} 失败", port_);
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    if (::listen(listen_fd_, 10) < 0) {
        LOG_ERROR("监听失败");
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    int flags = ::fcntl(listen_fd_, F_GETFL, 0);
    if (flags < 0) {
        LOG_ERROR("获取监听套接字标志失败");
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }
    if (::fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOG_ERROR("设置监听套接字非阻塞失败");
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    channel_ = new Channel(loop_, listen_fd_);
    channel_->setEvents(EPOLLIN);
    channel_->setReadCallback([this]() {
        handleRead();
    });
    loop_->addChannel(channel_);
}

Acceptor::~Acceptor() {
    if (channel_) {
        loop_->removeChannel(channel_);
        delete channel_;
        channel_ = nullptr;
    }
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
}

void Acceptor::setNewConnectionCallback(NewConnectionCallback cb) {
    callback_ = std::move(cb);
}

bool Acceptor::isValid() const {
    return listen_fd_ >= 0;
}

void Acceptor::handleRead() {
    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            LOG_ERROR("接受连接失败");
            break;
        }
        // 设置非阻塞
        int flags = ::fcntl(client_fd, F_GETFL, 0);
        ::fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

        if (callback_) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(client_addr.sin_addr), ip, INET_ADDRSTRLEN);
            callback_(client_fd, std::string(ip));
        } else {
            ::close(client_fd);
        }
    }
}

#include "tcp_connection.h"
#include "http_server.h"
#include "event_loop.h"
#include "channel.h"
#include "http_codec.h"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

TcpConnection::TcpConnection(HttpServer* server, EventLoop* loop, int fd, const std::string& ip)
    : server_(server),
      loop_(loop),
      fd_(fd),
      ip_(ip),
      closed_(false) {
    auto ch = std::make_unique<Channel>(loop_, fd_);
    ch->setEvents(EPOLLIN | EPOLLET);
    ch->setReadCallback([this]() {
        handleRead();
    });
    ch->setWriteCallback([this]() {
        handleWrite();
    });
    ch->setCloseCallback([this]() {
        handleClose();
    });
    loop_->addChannel(ch.get());
    channel_ = std::move(ch);
}

TcpConnection::~TcpConnection() {
    shutdown();
}

int TcpConnection::fd() const {
    return fd_;
}

bool TcpConnection::closed() const {
    return closed_;
}

void TcpConnection::handleRead() {
    if (closed_) {
        return;
    }

    char buffer[4096];
    while (true) {
        ssize_t n = ::recv(fd_, buffer, sizeof(buffer), 0);
        if (n > 0) {
            read_buffer_.append(buffer, static_cast<std::size_t>(n));
            // 限制请求体大小，防止内存耗尽 (10MB)
            if (read_buffer_.size() > 10 * 1024 * 1024) {
                HttpResponse resp;
                resp.status_code = 413;
                resp.status_text = "Payload Too Large";
                resp.body = R"({"error":"request entity too large"})";
                std::string resp_str = buildResponse(resp);
                appendResponse(resp_str);
                server_->closeConnection(fd_);
                return;
            }
        } else if (n == 0) {
            server_->closeConnection(fd_);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                server_->closeConnection(fd_);
                return;
            }
        }
    }

    bool loop = true;
    while (loop) {
        bool complete = false;
        bool bad = false;
        HttpRequest req = parseRequestFromBuffer(read_buffer_, complete, bad);
        if (bad) {
            HttpResponse resp;
            resp.status_code = 400;
            resp.status_text = "Bad Request";
            resp.body = R"({"error":"bad request"})";
            std::string resp_str = buildResponse(resp);
            appendResponse(resp_str);
            break;
        }
        if (!complete) {
            loop = false;
            break;
        }

        req.remote_ip = ip_;
        server_->handleHttpRequest(fd_, req);
    }
}

void TcpConnection::handleWrite() {
    if (closed_) {
        return;
    }

    if (!write_buffer_.empty()) {
        while (!write_buffer_.empty()) {
            ssize_t n = ::send(fd_, write_buffer_.data(), write_buffer_.size(), MSG_NOSIGNAL);
            if (n > 0) {
                write_buffer_.erase(0, static_cast<std::size_t>(n));
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    server_->closeConnection(fd_);
                    return;
                }
            }
        }
    }
    if (channel_) {
        uint32_t new_events = EPOLLIN | EPOLLET;
        if (!write_buffer_.empty()) {
            new_events |= EPOLLOUT;
        }
        channel_->setEvents(new_events);
        loop_->updateChannel(channel_.get());
    }
}

void TcpConnection::handleClose() {
    server_->closeConnection(fd_);
}

void TcpConnection::appendResponse(const std::string& data) {
    if (closed_) {
        return;
    }
    write_buffer_.append(data);
    if (channel_) {
        uint32_t new_events = channel_->events() | EPOLLOUT;
        channel_->setEvents(new_events);
        loop_->updateChannel(channel_.get());
    }
}

void TcpConnection::shutdown() {
    if (closed_) {
        return;
    }
    if (channel_) {
        loop_->removeChannel(channel_.get());
        channel_.reset();
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    closed_ = true;
}


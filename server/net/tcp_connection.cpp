#include "net/tcp_connection.h"
#include "http/http_server.h"
#include "net/event_loop.h"
#include "net/channel.h"
#include "http/http_codec.h"
#include "rtsp/rtsp_codec.h"
#include "utils/server_error.h"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

static constexpr size_t kMaxRequestSize = 10 * 1024 * 1024;

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
    channel_ = std::move(ch);
}

void TcpConnection::connectEstablished() {
    loop_->runInLoop([this]() {
        loop_->addChannel(channel_.get());
    });
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
            if (read_buffer_.size() > kMaxRequestSize) {
                HttpResponse resp = CreateErrorResponse(ErrorCode::PAYLOAD_TOO_LARGE);
                std::string resp_str = buildResponse(resp);
                appendResponse(resp_str);
                setCloseAfterWrite(true);
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

    if (protocol_ == Protocol::HTTP) {
        // Check for RTSP signature
        if (read_buffer_.find("RTSP/1.0") != std::string::npos) {
            protocol_ = Protocol::RTSP;
        } else {
            bool loop = true;
            while (loop) {
                bool complete = false;
                bool bad = false;
                HttpRequest req = parseRequestFromBuffer(read_buffer_, complete, bad);
                if (bad) {
                    HttpResponse resp = CreateErrorResponse(ErrorCode::INVALID_REQUEST);
                    std::string resp_str = buildResponse(resp);
                    appendResponse(resp_str);
                    setCloseAfterWrite(true);
                    break;
                }
                if (!complete) {
                    loop = false;
                    break;
                }

                req.remote_ip = ip_;
                
                // Check for WebSocket Upgrade
                auto itUpgrade = req.headers.find("Upgrade");
                if (itUpgrade != req.headers.end() && itUpgrade->second == "websocket") {
                    auto itKey = req.headers.find("Sec-WebSocket-Key");
                    if (itKey != req.headers.end()) {
                        std::string acceptKey = protocols::WebSocketCodec::computeAcceptKey(itKey->second);
                        
                        HttpResponse resp;
                        resp.status_code = 101;
                        resp.status_text = "Switching Protocols";
                        resp.headers["Upgrade"] = "websocket";
                        resp.headers["Connection"] = "Upgrade";
                        resp.headers["Sec-WebSocket-Accept"] = acceptKey;
                        
                        std::string respStr = buildResponse(resp);
                        send(respStr);
                        
                        protocol_ = Protocol::WEBSOCKET;
                        // Stop HTTP processing loop, proceed to WS processing if buffer has data
                        loop = false;
                    }
                } else {
                    server_->handleHttpRequest(shared_from_this(), req);
                }
            }
        }
    }
    
    if (protocol_ == Protocol::WEBSOCKET) {
        while (read_buffer_.size() > 0) {
            protocols::WebSocketFrame frame;
            std::vector<uint8_t> buf(read_buffer_.begin(), read_buffer_.end());
            int consumed = protocols::WebSocketCodec::parseFrame(buf, frame);
            
            if (consumed < 0) {
                server_->closeConnection(fd_);
                return;
            }
            if (consumed == 0) {
                break;
            }
            
            read_buffer_.erase(0, consumed);
            
            if (frame.opcode == protocols::WebSocketOpcode::CLOSE) {
                server_->closeConnection(fd_);
                return;
            } else if (frame.opcode == protocols::WebSocketOpcode::PING) {
                auto pong = protocols::WebSocketCodec::buildFrame(protocols::WebSocketOpcode::PONG, frame.payload);
                std::string pongStr(pong.begin(), pong.end());
                send(pongStr);
            } else {
                server_->handleWebSocketMessage(shared_from_this(), frame);
            }
        }
    }

    if (protocol_ == Protocol::RTSP) {
        while (read_buffer_.size() > 0) {
            protocols::RtspRequest req;
            size_t consumed = protocols::RtspCodec::parseRequest(read_buffer_, req);
            
            if (consumed == 0) {
                break;
            }
            
            read_buffer_.erase(0, consumed);
            
            server_->handleRtspMessage(shared_from_this(), req);
        }
    }
}

void TcpConnection::send(const std::string& data) {
    if (loop_->isInLoopThread()) {
        appendResponse(data);
    } else {
        loop_->runInLoop([self = shared_from_this(), data]() {
            self->appendResponse(data);
        });
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

    if (write_buffer_.empty() && close_after_write_) {
        server_->closeConnection(fd_);
        return;
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


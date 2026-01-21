#include "net/udp_socket.h"
#include "net/event_loop.h"
#include "logger.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

UdpSocket::UdpSocket(EventLoop* loop, int port)
    : loop_(loop), port_(port), fd_(::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)) {
    if (fd_ < 0) {
        LOG_ERROR("Failed to create UDP socket");
    }
}

UdpSocket::~UdpSocket() {
    if (channel_) {
        channel_->disableAll();
        channel_->remove();
    }
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

bool UdpSocket::bind() {
    if (fd_ < 0) return false;

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port_);

    if (::bind(fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("UDP bind failed on port {}", port_);
        return false;
    }

    channel_ = std::make_unique<Channel>(loop_, fd_);
    channel_->setReadCallback([this]() { handleRead(); });
    channel_->enableReading();
    
    LOG_INFO("UDP socket bound to port {}", port_);
    return true;
}

void UdpSocket::sendTo(const char* data, size_t len, const struct sockaddr_in& addr) {
    if (fd_ < 0) return;
    
    ssize_t n = ::sendto(fd_, data, len, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (n < 0) {
        LOG_ERROR("UDP sendto failed");
    }
}

void UdpSocket::sendTo(const std::string& data, const struct sockaddr_in& addr) {
    sendTo(data.data(), data.size(), addr);
}

void UdpSocket::handleRead() {
    char buffer[65536];
    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    
    ssize_t n = ::recvfrom(fd_, buffer, sizeof(buffer), 0, (struct sockaddr*)&addr, &addrLen);
    if (n > 0) {
        if (messageCallback_) {
            messageCallback_(buffer, n, addr);
        }
    } else if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("UDP recvfrom failed");
        }
    }
}

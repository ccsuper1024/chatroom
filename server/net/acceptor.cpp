#include "net/acceptor.h"
#include "net/event_loop.h"
#include "net/inet_address.h"
#include "logger.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

// Helper functions for socket operations
static int createNonblockingOrDie(sa_family_t family) {
    int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0) {
        LOG_FATAL("Acceptor::createNonblockingOrDie");
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocketFd_(createNonblockingOrDie(listenAddr.family())),
      acceptChannel_(loop, acceptSocketFd_),
      listening_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
    
    assert(idleFd_ >= 0);
    
    int opt = 1;
    ::setsockopt(acceptSocketFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (reuseport) {
        ::setsockopt(acceptSocketFd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    }
    
    if (::bind(acceptSocketFd_, (const struct sockaddr*)listenAddr.getSockAddr(), sizeof(struct sockaddr_in)) < 0) {
        LOG_FATAL("Acceptor::bind - port: {} - errno: {} ({})", listenAddr.toPort(), errno, strerror(errno));
    }
    
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    ::close(acceptSocketFd_);
    ::close(idleFd_);
}

void Acceptor::listen() {
    loop_->assertInLoopThread();
    listening_ = true;
    if (::listen(acceptSocketFd_, SOMAXCONN) < 0) {
        LOG_FATAL("Acceptor::listen");
    }
    acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
    loop_->assertInLoopThread();
    struct sockaddr_in peerAddr;
    socklen_t peerAddrLen = sizeof(peerAddr);
    // accept4 is Linux specific, but we are on Linux
    int connfd = ::accept4(acceptSocketFd_, (struct sockaddr*)&peerAddr, &peerAddrLen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            InetAddress inetPeerAddr(peerAddr);
            newConnectionCallback_(connfd, inetPeerAddr);
        } else {
            ::close(connfd);
        }
    } else {
        LOG_ERROR("Acceptor::handleRead");
        if (errno == EMFILE) {
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocketFd_, NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}

#include "net/tcp_server.h"
#include "net/acceptor.h"
#include "net/event_loop.h"
#include "net/event_loop_thread_pool.h"
#include "logger.h"

#include <stdio.h>

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const std::string& nameArg,
                     Option option)
    : loop_(loop),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      threadPool_(new EventLoopThreadPool(loop, name_)),
      started_(0),
      nextConnId_(1) {
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer() {
    loop_->assertInLoopThread();
    LOG_DEBUG("TcpServer::~TcpServer [{}] destructing", name_);
    
    for (auto& item : connections_) {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads) {
    assert(0 == started_);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() {
    if (started_.fetch_add(1) == 0) {
        threadPool_->start(nullptr);
        assert(!acceptor_->listening());
        loop_->runInLoop(
            std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
    loop_->assertInLoopThread();
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [{}] - new connection [{}] from {}", 
             name_, connName, peerAddr.toIpPort());

    // Construct local address (for completeness)
    struct sockaddr_in local;
    socklen_t addrlen = sizeof(local);
    ::getsockname(sockfd, (struct sockaddr*)&local, &addrlen);
    InetAddress localAddr(local);

    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
        
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    // This callback comes from TcpConnection::handleClose() which is in ioLoop
    // We need to move to main loop (where connections_ map lives)
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
    loop_->assertInLoopThread();
    LOG_INFO("TcpServer::removeConnectionInLoop [{}] - connection {}", name_, conn->name());
    
    size_t erased = connections_.erase(conn->name());
    if (erased == 0) {
        LOG_WARN("TcpServer::removeConnectionInLoop [{}] - connection not found", name_);
    }
    
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
}

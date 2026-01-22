#pragma once

#include <memory>
#include <string>
#include <vector>
#include <any>
#include <atomic>
#include "net/callbacks.h"
#include "net/buffer.h"
#include "net/inet_address.h"

class EventLoop;
class Channel;
class Socket;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    friend class ChatRoomServerTest;

    TcpConnection(EventLoop* loop,
                  const std::string& name,
                  int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }

    // Send data (thread-safe)
    void send(const std::string& message);
    void send(Buffer* message);

    void shutdown();
    void forceClose();
    void setCloseAfterWrite(bool close) { if (close) shutdown(); }

    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) { 
        highWaterMarkCallback_ = cb; 
        highWaterMark_ = highWaterMark;
    }
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }

    // Context management
    void setContext(const std::any& context) { context_ = context; }
    const std::any& getContext() const { return context_; }
    std::any* getMutableContext() { return &context_; }

    // Internal use
    void connectEstablished();
    void connectDestroyed();

    // Changed to public for testing
    void handleRead();
    void handleWrite();

private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

    // void handleRead(); // Moved to public
    // void handleWrite(); // Moved to public
    void handleClose();
    void handleError();
    
    void sendInLoop(const std::string& message);
    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();
    void setState(StateE s) { state_ = s; }

    EventLoop* loop_;
    const std::string name_;
    std::atomic<StateE> state_;
    bool reading_;
    
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;
    
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    
    size_t highWaterMark_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;
    std::any context_;
};



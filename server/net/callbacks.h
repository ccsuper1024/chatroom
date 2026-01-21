#pragma once

#include <memory>
#include <functional>
#include "net/timestamp.h"

using Timestamp = net::Timestamp;

class Buffer;
class TcpConnection;
class InetAddress;
class UdpServer; // Forward declaration

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

// TCP Callbacks
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, Timestamp)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;

// UDP Callbacks
// 注意：UDP 是无连接的，所以没有 Connection 对象，只有 Server 指针和对端地址
using UdpMessageCallback = std::function<void(UdpServer*, Buffer*, const InetAddress&)>;

// Timer Callback
using TimerCallback = std::function<void()>;

#pragma once

#include <functional>
#include <memory>
#include "net/tcp_server.h"
#include "net/event_loop.h"
#include "rtsp/rtsp_codec.h"

class TcpConnection;

using RtspHandler = std::function<void(const std::shared_ptr<TcpConnection>&, const protocols::RtspRequest&)>;

class RtspServer {
public:
    RtspServer(EventLoop* loop, int port);
    ~RtspServer();

    void setRtspHandler(RtspHandler handler);
    void start();
    
    /**
     * @brief 获取监听端口
     */
    int port() const { return port_; }

private:
    void onConnection(const std::shared_ptr<TcpConnection>& conn);
    void onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buf, Timestamp time);

    TcpServer server_;
    int port_;
    RtspHandler rtsp_handler_;
};

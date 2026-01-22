#pragma once

#include <functional>
#include <memory>
#include "net/tcp_server.h"
#include "net/event_loop.h"
#include "sip/sip_codec.h"

class TcpConnection;

using SipHandler = std::function<void(const std::shared_ptr<TcpConnection>&, const SipRequest&, const std::string&)>;

class SipServer {
public:
    SipServer(EventLoop* loop, int port);
    ~SipServer();

    void setSipHandler(SipHandler handler);
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
    SipHandler sip_handler_;
};

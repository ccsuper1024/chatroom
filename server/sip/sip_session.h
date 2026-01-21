#pragma once

#include "net/protocol_session.h"
#include "sip/sip_codec.h"
#include <memory>
#include <string>

class HttpServer;
class TcpConnection;

class SipSession : public IProtocolSession {
public:
    SipSession(HttpServer* server, std::shared_ptr<TcpConnection> conn);
    ~SipSession() override;

    ProtocolType type() const override { return ProtocolType::SIP; }
    void onData(const char* data, std::size_t len) override;

private:
    void handleMessage(const std::string& raw_msg, const SipRequest& req);

    HttpServer* server_;
    std::weak_ptr<TcpConnection> conn_;
    std::string buffer_;
};

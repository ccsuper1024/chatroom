#pragma once

#include <memory>
#include <string>
#include "net/protocol_session.h"
#include "rtsp/rtsp_codec.h"

class HttpServer;
class TcpConnection;

class RtspSession : public IProtocolSession {
public:
    RtspSession(HttpServer* server, std::shared_ptr<TcpConnection> conn);

    ProtocolType type() const override;
    void onData(const char* data, std::size_t len) override;

private:
    HttpServer* server_;
    std::shared_ptr<TcpConnection> conn_;
    std::string buffer_;
};


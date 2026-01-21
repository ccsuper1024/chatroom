#pragma once

#include "net/protocol_session.h"
#include <memory>
#include <string>

class HttpServer;
class TcpConnection;

class FtpSession : public IProtocolSession {
public:
    FtpSession(HttpServer* server, std::shared_ptr<TcpConnection> conn);
    ~FtpSession() override;

    ProtocolType type() const override { return ProtocolType::FTP; }
    void onData(const char* data, std::size_t len) override;

private:
    void handleCommand(const std::string& command);

    HttpServer* server_;
    std::weak_ptr<TcpConnection> conn_;
    std::string buffer_;
};

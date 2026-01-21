#pragma once

#include <memory>
#include <string>
#include "net/protocol_session.h"
#include "http/http_codec.h"
#include "websocket/websocket_codec.h"

class HttpServer;
class TcpConnection;

class HttpSession : public IProtocolSession {
public:
    HttpSession(HttpServer* server, std::shared_ptr<TcpConnection> conn);

    ProtocolType type() const override;
    void onData(const char* data, std::size_t len) override;

private:
    enum class Mode {
        HTTP,
        WEBSOCKET
    };

    void handleHttp();
    void handleWebSocket();

    HttpServer* server_;
    std::shared_ptr<TcpConnection> conn_;
    std::string buffer_;
    Mode mode_;
};


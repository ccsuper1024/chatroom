#include "rtsp/rtsp_session.h"
#include "http/http_server.h"
#include "net/tcp_connection.h"
#include <vector>

RtspSession::RtspSession(HttpServer* server, std::shared_ptr<TcpConnection> conn)
    : server_(server),
      conn_(std::move(conn)) {
}

ProtocolType RtspSession::type() const {
    return ProtocolType::RTSP;
}

void RtspSession::onData(const char* data, std::size_t len) {
    if (!data || len == 0) {
        return;
    }

    buffer_.append(data, len);

    while (!buffer_.empty()) {
        protocols::RtspRequest req;
        std::size_t consumed = protocols::RtspCodec::parseRequest(buffer_, req);

        if (consumed == 0) {
            break;
        }

        buffer_.erase(0, consumed);
        server_->handleRtspMessage(conn_, req);
    }
}


#include "sip/sip_session.h"
#include "net/tcp_connection.h"
#include "http/http_server.h"
#include "logger.h"
#include "sip/sip_codec.h"

SipSession::SipSession(HttpServer* server, std::shared_ptr<TcpConnection> conn)
    : server_(server), conn_(conn) {
}

SipSession::~SipSession() {
}

void SipSession::onData(const char* data, std::size_t len) {
    buffer_.append(data, len);
    
    while (true) {
        SipRequest req;
        size_t consumed = SipCodec::parseRequest(buffer_, req);
        if (consumed == 0) {
            break;
        }
        
        handleMessage(buffer_.substr(0, consumed), req);
        buffer_.erase(0, consumed);
    }
}

void SipSession::handleMessage(const std::string& raw_msg, const SipRequest& req) {
    LOG_INFO("Received SIP message: {} {}", SipCodec::methodToString(req.method), req.uri);
    
    if (server_) {
        if (auto conn = conn_.lock()) {
            server_->handleSipMessage(conn, req, raw_msg);
            return;
        }
    }

    // Fallback: Echo back 200 OK
    std::string response = SipCodec::buildResponse(200, "OK", req);
    if (auto conn = conn_.lock()) {
        conn->send(response);
    }
}

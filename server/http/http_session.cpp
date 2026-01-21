#include "http/http_session.h"
#include "http/http_server.h"
#include "net/tcp_connection.h"
#include "utils/server_error.h"
#include <vector>

namespace {
constexpr std::size_t kMaxRequestSize = 10 * 1024 * 1024;
}

HttpSession::HttpSession(HttpServer* server, std::shared_ptr<TcpConnection> conn)
    : server_(server),
      conn_(std::move(conn)),
      mode_(Mode::HTTP) {
}

ProtocolType HttpSession::type() const {
    return ProtocolType::HTTP;
}

void HttpSession::onData(const char* data, std::size_t len) {
    if (!data || len == 0) {
        return;
    }

    buffer_.append(data, len);
    if (buffer_.size() > kMaxRequestSize) {
        HttpResponse resp = CreateErrorResponse(ErrorCode::PAYLOAD_TOO_LARGE);
        std::string resp_str = buildResponse(resp);
        conn_->send(resp_str);
        conn_->setCloseAfterWrite(true);
        buffer_.clear();
        return;
    }

    while (true) {
        if (mode_ == Mode::HTTP) {
            handleHttp();
            if (mode_ == Mode::HTTP) {
                break;
            }
        } else {
            handleWebSocket();
            break;
        }
    }
}

void HttpSession::handleHttp() {
    bool loop = true;
    while (loop) {
        bool complete = false;
        bool bad = false;
        HttpRequest req = parseRequestFromBuffer(buffer_, complete, bad);
        if (bad) {
            HttpResponse resp = CreateErrorResponse(ErrorCode::INVALID_REQUEST);
            std::string resp_str = buildResponse(resp);
            conn_->send(resp_str);
            conn_->setCloseAfterWrite(true);
            buffer_.clear();
            return;
        }
        if (!complete) {
            loop = false;
            break;
        }

        req.remote_ip = conn_->ip();

        auto itUpgrade = req.headers.find("Upgrade");
        if (itUpgrade != req.headers.end() && itUpgrade->second == "websocket") {
            auto itKey = req.headers.find("Sec-WebSocket-Key");
            if (itKey != req.headers.end()) {
                std::string acceptKey = protocols::WebSocketCodec::computeAcceptKey(itKey->second);

                HttpResponse resp;
                resp.status_code = 101;
                resp.status_text = "Switching Protocols";
                resp.headers["Upgrade"] = "websocket";
                resp.headers["Connection"] = "Upgrade";
                resp.headers["Sec-WebSocket-Accept"] = acceptKey;

                std::string respStr = buildResponse(resp);
                conn_->send(respStr);

                mode_ = Mode::WEBSOCKET;
                loop = false;
            }
        } else {
            server_->handleHttpRequest(conn_, req);
        }
    }
}

void HttpSession::handleWebSocket() {
    while (!buffer_.empty()) {
        protocols::WebSocketFrame frame;
        std::vector<uint8_t> buf(buffer_.begin(), buffer_.end());
        int consumed = protocols::WebSocketCodec::parseFrame(buf, frame);

        if (consumed < 0) {
            server_->closeConnection(conn_->fd());
            return;
        }
        if (consumed == 0) {
            break;
        }

        buffer_.erase(0, static_cast<std::size_t>(consumed));

        if (frame.opcode == protocols::WebSocketOpcode::CLOSE) {
            server_->closeConnection(conn_->fd());
            return;
        } else if (frame.opcode == protocols::WebSocketOpcode::PING) {
            auto pong = protocols::WebSocketCodec::buildFrame(protocols::WebSocketOpcode::PONG, frame.payload);
            std::string pongStr(pong.begin(), pong.end());
            conn_->send(pongStr);
        } else {
            server_->handleWebSocketMessage(conn_, frame);
        }
    }
}

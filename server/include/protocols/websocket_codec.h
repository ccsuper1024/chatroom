#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "protocols/crypto_utils.h"

namespace protocols {

enum class WebSocketOpcode {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA,
    UNKNOWN = 0xF
};

struct WebSocketFrame {
    bool fin;
    WebSocketOpcode opcode;
    bool masked;
    std::vector<uint8_t> payload;
};

class WebSocketCodec {
public:
    // Constants
    static constexpr char MAGIC_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    // Handshake
    static std::string computeAcceptKey(const std::string& client_key);
    static std::string buildHandshakeResponse(const std::string& accept_key);

    // Frame Parsing
    // Returns bytes consumed. 0 if incomplete frame. -1 if error.
    static int parseFrame(const std::vector<uint8_t>& buffer, WebSocketFrame& out_frame);

    // Frame Building
    static std::vector<uint8_t> buildFrame(const WebSocketFrame& frame);
    static std::vector<uint8_t> buildFrame(WebSocketOpcode opcode, const std::string& payload, bool fin = true);
    static std::vector<uint8_t> buildFrame(WebSocketOpcode opcode, const std::vector<uint8_t>& payload, bool fin = true);
};

} // namespace protocols

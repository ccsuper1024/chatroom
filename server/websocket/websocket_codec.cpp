#include "websocket/websocket_codec.h"
#include <sstream>
#include <algorithm>
#include <arpa/inet.h> // for htons, htonl, ntohs, ntohl (on Linux)
#include <cstring> // for memcpy

namespace protocols {

std::string WebSocketCodec::computeAcceptKey(const std::string& client_key) {
    std::string combined = client_key + MAGIC_GUID;
    std::vector<unsigned char> sha1_hash = CryptoUtils::sha1(combined);
    return CryptoUtils::base64Encode(sha1_hash);
}

std::string WebSocketCodec::buildHandshakeResponse(const std::string& accept_key) {
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << accept_key << "\r\n"
             << "\r\n";
    return response.str();
}

int WebSocketCodec::parseFrame(const std::vector<uint8_t>& buffer, WebSocketFrame& out_frame) {
    if (buffer.size() < 2) return 0; // Need at least 2 bytes

    size_t offset = 0;
    uint8_t byte1 = buffer[offset++];
    uint8_t byte2 = buffer[offset++];

    out_frame.fin = (byte1 & 0x80) != 0;
    out_frame.opcode = static_cast<WebSocketOpcode>(byte1 & 0x0F);
    out_frame.masked = (byte2 & 0x80) != 0;
    uint64_t payload_len = byte2 & 0x7F;

    if (payload_len == 126) {
        if (buffer.size() < offset + 2) return 0;
        uint16_t len16;
        std::memcpy(&len16, &buffer[offset], 2);
        payload_len = ntohs(len16);
        offset += 2;
    } else if (payload_len == 127) {
        if (buffer.size() < offset + 8) return 0;
        uint64_t len64;
        std::memcpy(&len64, &buffer[offset], 8);
        // ntohll is not standard, manual conversion for big-endian network order
        // Assuming we are on Little Endian (x86), we need to swap bytes
        // Or better, read byte by byte
        uint64_t temp = 0;
        for(int i=0; i<8; ++i) {
             temp = (temp << 8) | buffer[offset + i];
        }
        payload_len = temp;
        offset += 8;
    }

    uint8_t masking_key[4];
    if (out_frame.masked) {
        if (buffer.size() < offset + 4) return 0;
        std::memcpy(masking_key, &buffer[offset], 4);
        offset += 4;
    }

    if (buffer.size() < offset + payload_len) return 0;

    out_frame.payload.resize(payload_len);
    if (payload_len > 0) {
        std::memcpy(out_frame.payload.data(), &buffer[offset], payload_len);
        if (out_frame.masked) {
            for (size_t i = 0; i < payload_len; ++i) {
                out_frame.payload[i] ^= masking_key[i % 4];
            }
        }
    }
    offset += payload_len;

    return static_cast<int>(offset);
}

std::vector<uint8_t> WebSocketCodec::buildFrame(const WebSocketFrame& frame) {
    std::vector<uint8_t> buffer;
    
    uint8_t byte1 = (frame.fin ? 0x80 : 0x00) | (static_cast<uint8_t>(frame.opcode) & 0x0F);
    buffer.push_back(byte1);

    uint8_t byte2 = frame.masked ? 0x80 : 0x00;
    size_t len = frame.payload.size();

    if (len <= 125) {
        byte2 |= static_cast<uint8_t>(len);
        buffer.push_back(byte2);
    } else if (len <= 65535) {
        byte2 |= 126;
        buffer.push_back(byte2);
        uint16_t len16 = htons(static_cast<uint16_t>(len));
        uint8_t* p = reinterpret_cast<uint8_t*>(&len16);
        buffer.push_back(p[0]);
        buffer.push_back(p[1]);
    } else {
        byte2 |= 127;
        buffer.push_back(byte2);
        // Manual big-endian push for 64-bit length
        for (int i = 7; i >= 0; --i) {
            buffer.push_back((len >> (i * 8)) & 0xFF);
        }
    }

    // Server usually doesn't mask frames sent to client, but spec allows it.
    // We skip masking for simplicity as it's not required for server-to-client.
    
    buffer.insert(buffer.end(), frame.payload.begin(), frame.payload.end());
    return buffer;
}

std::vector<uint8_t> WebSocketCodec::buildFrame(WebSocketOpcode opcode, const std::string& payload, bool fin) {
    WebSocketFrame frame;
    frame.fin = fin;
    frame.opcode = opcode;
    frame.masked = false;
    frame.payload.assign(payload.begin(), payload.end());
    return buildFrame(frame);
}

std::vector<uint8_t> WebSocketCodec::buildFrame(WebSocketOpcode opcode, const std::vector<uint8_t>& payload, bool fin) {
    WebSocketFrame frame;
    frame.fin = fin;
    frame.opcode = opcode;
    frame.masked = false;
    frame.payload = payload;
    return buildFrame(frame);
}

} // namespace protocols

#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <arpa/inet.h>

namespace protocols {

// RTP Header (RFC 3550)
struct RtpHeader {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t cc:4;      // CSRC count
    uint8_t x:1;       // Extension
    uint8_t p:1;       // Padding
    uint8_t version:2; // Version
    
    uint8_t pt:7;      // Payload Type
    uint8_t m:1;       // Marker
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint8_t version:2;
    uint8_t p:1;
    uint8_t x:1;
    uint8_t cc:4;
    
    uint8_t m:1;
    uint8_t pt:7;
#endif
    uint16_t seq;      // Sequence Number
    uint32_t ts;       // Timestamp
    uint32_t ssrc;     // Synchronization Source Identifier
};

// RTCP Header (RFC 3550)
struct RtcpHeader {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t rc:5;      // Report Count
    uint8_t p:1;       // Padding
    uint8_t version:2; // Version
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint8_t version:2;
    uint8_t p:1;
    uint8_t rc:5;
#endif
    uint8_t pt;        // Packet Type
    uint16_t length;   // Length
};

class RtpPacket {
public:
    RtpHeader header;
    std::vector<uint8_t> payload;
    
    // Serializes the packet into a buffer
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buffer;
        buffer.resize(12 + payload.size()); // 12 bytes header
        
        // Manual serialization to ensure network byte order (Big Endian)
        buffer[0] = (header.version << 6) | (header.p << 5) | (header.x << 4) | header.cc;
        buffer[1] = (header.m << 7) | (header.pt & 0x7F);
        
        uint16_t seq_be = htons(header.seq);
        uint32_t ts_be = htonl(header.ts);
        uint32_t ssrc_be = htonl(header.ssrc);
        
        // Copy using simple assignment or memcpy
        buffer[2] = (seq_be >> 0) & 0xFF; // Wait, htons/htonl returns Big Endian.
        // We need to put them into memory.
        // Direct pointer cast is easier but assumes alignment.
        // Let's do byte assignment for safety.
        
        // Actually, buffer is vector<uint8_t>, so we can just:
        *(uint16_t*)&buffer[2] = seq_be;
        *(uint32_t*)&buffer[4] = ts_be;
        *(uint32_t*)&buffer[8] = ssrc_be;
        
        if (!payload.empty()) {
            std::copy(payload.begin(), payload.end(), buffer.begin() + 12);
        }
        return buffer;
    }
};

} // namespace protocols

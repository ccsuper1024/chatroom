#include <gtest/gtest.h>
#include "protocols/websocket_codec.h"
#include "protocols/rtsp_codec.h"
#include "protocols/rtp_rtcp.h"
#include "protocols/crypto_utils.h"

using namespace protocols;

// --- WebSocket Tests ---

TEST(WebSocketTest, AcceptKeyComputation) {
    // Example from RFC 6455
    std::string client_key = "dGhlIHNhbXBsZSBub25jZQ==";
    std::string accept_key = WebSocketCodec::computeAcceptKey(client_key);
    EXPECT_EQ(accept_key, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

TEST(WebSocketTest, BuildFrameText) {
    std::string text = "Hello";
    std::vector<uint8_t> frame = WebSocketCodec::buildFrame(WebSocketOpcode::TEXT, text);
    
    // FIN=1, Opcode=1 (Text) -> 0x81
    EXPECT_EQ(frame[0], 0x81);
    // Mask=0, Len=5 -> 0x05
    EXPECT_EQ(frame[1], 0x05);
    // Payload
    EXPECT_EQ(frame[2], 'H');
    EXPECT_EQ(frame[3], 'e');
    EXPECT_EQ(frame[4], 'l');
    EXPECT_EQ(frame[5], 'l');
    EXPECT_EQ(frame[6], 'o');
}

TEST(WebSocketTest, ParseFrameMasked) {
    // Client to Server frame "Hello" (masked)
    // FIN=1, Opcode=1 -> 0x81
    // Mask=1, Len=5 -> 0x85
    // Masking Key = 0x37 0xfa 0x21 0x3d (example)
    // Encoded Payload:
    // H (0x48) ^ 0x37 = 0x7F
    // e (0x65) ^ 0xfa = 0x9F
    // l (0x6c) ^ 0x21 = 0x4D
    // l (0x6c) ^ 0x3d = 0x51
    // o (0x6f) ^ 0x37 = 0x58
    
    std::vector<uint8_t> buffer = {
        0x81, 0x85, 
        0x37, 0xfa, 0x21, 0x3d, 
        0x7F, 0x9F, 0x4D, 0x51, 0x58
    };

    WebSocketFrame frame;
    int consumed = WebSocketCodec::parseFrame(buffer, frame);
    
    EXPECT_EQ(consumed, buffer.size());
    EXPECT_TRUE(frame.fin);
    EXPECT_EQ(frame.opcode, WebSocketOpcode::TEXT);
    EXPECT_TRUE(frame.masked);
    
    std::string payload_str(frame.payload.begin(), frame.payload.end());
    EXPECT_EQ(payload_str, "Hello");
}

// --- RTSP Tests ---

TEST(RtspTest, ParseRequest) {
    std::string raw_req = 
        "SETUP rtsp://example.com/media.mp4 RTSP/1.0\r\n"
        "CSeq: 302\r\n"
        "Transport: RTP/AVP;unicast;client_port=4588-4589\r\n"
        "\r\n";
        
    RtspRequest req;
    size_t consumed = RtspCodec::parseRequest(raw_req, req);
    
    EXPECT_GT(consumed, 0);
    EXPECT_EQ(req.method, RtspMethod::SETUP);
    EXPECT_EQ(req.url, "rtsp://example.com/media.mp4");
    EXPECT_EQ(req.version, "RTSP/1.0");
    EXPECT_EQ(req.cseq, 302);
    EXPECT_EQ(req.headers["Transport"], "RTP/AVP;unicast;client_port=4588-4589");
}

TEST(RtspTest, BuildResponse) {
    RtspResponse resp;
    resp.status_code = 200;
    resp.status_text = "OK";
    resp.cseq = 302;
    resp.headers["Session"] = "12345678";
    
    std::string raw_resp = RtspCodec::buildResponse(resp);
    
    EXPECT_NE(raw_resp.find("RTSP/1.0 200 OK"), std::string::npos);
    EXPECT_NE(raw_resp.find("CSeq: 302"), std::string::npos);
    EXPECT_NE(raw_resp.find("Session: 12345678"), std::string::npos);
}

// --- RTP Tests ---

TEST(RtpTest, SerializePacket) {
    RtpPacket packet;
    packet.header.version = 2;
    packet.header.p = 0;
    packet.header.x = 0;
    packet.header.cc = 0;
    packet.header.m = 1;
    packet.header.pt = 96; // Dynamic
    packet.header.seq = 100;
    packet.header.ts = 123456;
    packet.header.ssrc = 0xDEADBEEF;
    
    packet.payload = {0x01, 0x02, 0x03, 0x04};
    
    std::vector<uint8_t> buffer = packet.serialize();
    
    EXPECT_EQ(buffer.size(), 12 + 4);
    // Byte 0: V=2(10), P=0, X=0, CC=0 -> 10000000 -> 0x80
    EXPECT_EQ(buffer[0], 0x80);
    // Byte 1: M=1, PT=96(1100000) -> 11100000 -> 0xE0
    EXPECT_EQ(buffer[1], 0xE0);
    
    // Check payload
    EXPECT_EQ(buffer[12], 0x01);
    EXPECT_EQ(buffer[15], 0x04);
}

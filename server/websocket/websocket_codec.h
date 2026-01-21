#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "utils/crypto_utils.h"

namespace protocols {

/**
 * @brief WebSocket操作码枚举
 */
enum class WebSocketOpcode {
    CONTINUATION = 0x0, ///< 连续帧
    TEXT = 0x1,         ///< 文本帧
    BINARY = 0x2,       ///< 二进制帧
    CLOSE = 0x8,        ///< 关闭连接帧
    PING = 0x9,         ///< Ping帧
    PONG = 0xA,         ///< Pong帧
    UNKNOWN = 0xF       ///< 未知/保留
};

/**
 * @brief WebSocket数据帧结构体
 */
struct WebSocketFrame {
    bool fin;                       ///< 是否为最后一帧
    WebSocketOpcode opcode;         ///< 操作码
    bool masked;                    ///< 是否包含掩码（客户端发往服务端必须包含）
    std::string_view payload;       ///< 负载数据 (Zero-copy view into Buffer)
};

/**
 * @brief WebSocket编解码器
 * 
 * 负责WebSocket握手处理以及数据帧的解析与构建。
 * 遵循 RFC 6455 标准。
 */
class WebSocketCodec {
public:
    // Constants
    /**
     * @brief WebSocket握手魔数 (RFC 6455)
     * @param client_key 客户端请求头中的 Sec-WebSocket-Key
     * @return std::string 计算出的 Sec-WebSocket-Accept 值
     */
    static constexpr char MAGIC_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    // Handshake
    static std::string computeAcceptKey(const std::string& client_key);

    /**
     * @brief 构建握手响应报文
     * @param accept_key 计算出的 Accept Key
     * @return std::string 完整的HTTP响应报文
     */
    static std::string buildHandshakeResponse(const std::string& accept_key);

    // Frame Parsing
    /**
     * @brief 解析WebSocket数据帧 (In-Place Unmasking)
     * @param data 数据指针 (Mutable, will be modified if masked)
     * @param len 数据长度
     * @param out_frame [out] 解析出的帧结构
     * @return int 解析消耗的字节数。
     *         0 表示数据不足以解析完整帧；
     *         -1 表示解析错误（如协议违规）。
     */
    static int parseFrame(uint8_t* data, size_t len, WebSocketFrame& out_frame);
    
    // Legacy support (Deprecated for zero-copy)
    static int parseFrame(const std::vector<uint8_t>& buffer, WebSocketFrame& out_frame) {
        // Must copy to allow modification for unmasking or fail if const
        // This legacy method cannot support zero-copy safely with unmasking
        // We removed it or keep it but it won't be zero-copy. 
        // For now, let's remove it to force update or implement via copy.
        std::vector<uint8_t> copy = buffer;
        return parseFrame(copy.data(), copy.size(), out_frame);
    }

    // Frame Building
    /**
     * @brief 构建WebSocket数据帧
     * @param frame 帧结构体
     * @return std::vector<uint8_t> 序列化后的二进制数据
     */
    static std::vector<uint8_t> buildFrame(const WebSocketFrame& frame);

    /**
     * @brief 构建WebSocket数据帧（字符串负载）
     * @param opcode 操作码
     * @param payload 字符串负载
     * @param fin 是否结束帧 (默认 true)
     * @return std::vector<uint8_t> 序列化后的二进制数据
     */
    static std::vector<uint8_t> buildFrame(WebSocketOpcode opcode, const std::string& payload, bool fin = true);

    /**
     * @brief 构建WebSocket数据帧（二进制负载）
     * @param opcode 操作码
     * @param payload 二进制负载
     * @param fin 是否结束帧 (默认 true)
     * @return std::vector<uint8_t> 序列化后的二进制数据
     */
    static std::vector<uint8_t> buildFrame(WebSocketOpcode opcode, const std::vector<uint8_t>& payload, bool fin = true);
};

} // namespace protocols

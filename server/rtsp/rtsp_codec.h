#pragma once

#include <string>
#include <map>
#include <vector>
#include "net/buffer.h"

namespace protocols {

/**
 * @brief RTSP方法枚举
 */
enum class RtspMethod {
    OPTIONS,    ///< 获取服务器支持的方法
    DESCRIBE,   ///< 获取媒体描述信息 (SDP)
    SETUP,      ///< 建立传输连接
    PLAY,       ///< 开始播放
    PAUSE,      ///< 暂停播放
    TEARDOWN,   ///< 终止会话
    UNKNOWN     ///< 未知方法
};

/**
 * @brief RTSP请求结构体
 */
struct RtspRequest {
    RtspMethod method;                      ///< 请求方法
    std::string url;                        ///< 请求URL
    std::string version;                    ///< 协议版本 (e.g. "RTSP/1.0")
    int cseq = -1;                          ///< 序列号 (CSeq)
    std::map<std::string, std::string> headers; ///< 请求头集合
    std::string body;                       ///< 请求体
};

/**
 * @brief RTSP响应结构体
 */
struct RtspResponse {
    std::string version = "RTSP/1.0";       ///< 协议版本
    int status_code = 200;                  ///< 状态码
    std::string status_text = "OK";         ///< 状态描述
    int cseq = -1;                          ///< 序列号 (对应请求的CSeq)
    std::map<std::string, std::string> headers; ///< 响应头集合
    std::string body;                       ///< 响应体
};

/**
 * @brief RTSP协议编解码器
 * 
 * 负责RTSP请求的解析和响应的构建。
 */
class RtspCodec {
public:
    /**
     * @brief 解析RTSP请求 (Zero-copy optimization)
     * @param buf 输入缓冲区
     * @param out_request [out] 解析后的请求结构体
     * @return size_t 消耗的字节数。
     *         0 表示数据不足以解析完整请求；
     *         >0 表示解析成功并消耗的字节数。
     */
    static size_t parseRequest(Buffer* buf, RtspRequest& out_request);

    /**
     * @brief 构建RTSP响应报文
     * @param response 响应结构体
     * @return std::string 序列化后的RTSP响应字符串
     */
    static std::string buildResponse(const RtspResponse& response);

    /**
     * @brief 将字符串转换为RtspMethod枚举
     * @param method_str 方法字符串 (如 "SETUP")
     * @return RtspMethod 对应的枚举值
     */
    static RtspMethod parseMethod(const std::string& method_str);

    /**
     * @brief 将RtspMethod枚举转换为字符串
     * @param method 方法枚举
     * @return std::string 方法字符串
     */
    static std::string methodToString(RtspMethod method);
};

} // namespace protocols

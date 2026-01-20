#pragma once

#include <string>
#include <map>
#include <vector>

namespace protocols {

enum class RtspMethod {
    OPTIONS,
    DESCRIBE,
    SETUP,
    PLAY,
    PAUSE,
    TEARDOWN,
    UNKNOWN
};

struct RtspRequest {
    RtspMethod method;
    std::string url;
    std::string version; // e.g. "RTSP/1.0"
    int cseq = -1;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct RtspResponse {
    std::string version = "RTSP/1.0";
    int status_code = 200;
    std::string status_text = "OK";
    int cseq = -1;
    std::map<std::string, std::string> headers;
    std::string body;
};

class RtspCodec {
public:
    // Parses RTSP request from buffer.
    // Returns number of bytes consumed. 0 if incomplete.
    static size_t parseRequest(const std::string& buffer, RtspRequest& out_request);

    static std::string buildResponse(const RtspResponse& response);

    static RtspMethod parseMethod(const std::string& method_str);
    static std::string methodToString(RtspMethod method);
};

} // namespace protocols

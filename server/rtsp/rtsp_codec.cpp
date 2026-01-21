#include "rtsp/rtsp_codec.h"
#include <sstream>
#include <algorithm>
#include <string_view>

namespace protocols {

RtspMethod RtspCodec::parseMethod(const std::string& method_str) {
    if (method_str == "OPTIONS") return RtspMethod::OPTIONS;
    if (method_str == "DESCRIBE") return RtspMethod::DESCRIBE;
    if (method_str == "SETUP") return RtspMethod::SETUP;
    if (method_str == "PLAY") return RtspMethod::PLAY;
    if (method_str == "PAUSE") return RtspMethod::PAUSE;
    if (method_str == "TEARDOWN") return RtspMethod::TEARDOWN;
    return RtspMethod::UNKNOWN;
}

std::string RtspCodec::methodToString(RtspMethod method) {
    switch (method) {
        case RtspMethod::OPTIONS: return "OPTIONS";
        case RtspMethod::DESCRIBE: return "DESCRIBE";
        case RtspMethod::SETUP: return "SETUP";
        case RtspMethod::PLAY: return "PLAY";
        case RtspMethod::PAUSE: return "PAUSE";
        case RtspMethod::TEARDOWN: return "TEARDOWN";
        default: return "UNKNOWN";
    }
}

size_t RtspCodec::parseRequest(Buffer* buf, RtspRequest& out_request) {
    const char* begin = buf->peek();
    const char* end = buf->beginWrite();

    // Find double CRLF
    const char crlf2[] = "\r\n\r\n";
    const char* headersEnd = std::search(begin, end, crlf2, crlf2 + 4);

    if (headersEnd == end) {
        return 0; // Incomplete
    }

    std::string_view headerPart(begin, headersEnd - begin);
    
    size_t lineStart = 0;
    size_t lineEnd = headerPart.find("\r\n");

    // Request line
    if (lineEnd == std::string_view::npos) {
        // Should be impossible if \r\n\r\n found, unless it's just empty lines?
        return 0; 
    }

    std::string_view requestLine = headerPart.substr(0, lineEnd);
    
    // Parse Method URL Version
    // "OPTIONS rtsp://example.com/media.mp4 RTSP/1.0"
    size_t methodEnd = requestLine.find(' ');
    if (methodEnd == std::string_view::npos) return 0; // Invalid
    
    std::string methodStr(requestLine.substr(0, methodEnd));
    out_request.method = parseMethod(methodStr);
    
    size_t urlStart = methodEnd + 1;
    size_t urlEnd = requestLine.find(' ', urlStart);
    if (urlEnd == std::string_view::npos) return 0; // Invalid
    
    out_request.url = std::string(requestLine.substr(urlStart, urlEnd - urlStart));
    out_request.version = std::string(requestLine.substr(urlEnd + 1));

    // Headers
    lineStart = lineEnd + 2;
    size_t contentLength = 0;

    while (lineStart < headerPart.size()) {
        lineEnd = headerPart.find("\r\n", lineStart);
        if (lineEnd == std::string_view::npos) lineEnd = headerPart.size();
        
        std::string_view line = headerPart.substr(lineStart, lineEnd - lineStart);
        size_t colon = line.find(':');
        if (colon != std::string_view::npos) {
            std::string key(line.substr(0, colon));
            std::string_view val = line.substr(colon + 1);
            while(!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.remove_prefix(1);
            
            out_request.headers[key] = std::string(val);
            
            if (key == "CSeq") {
                try {
                    out_request.cseq = std::stoi(std::string(val));
                } catch(...) {}
            }
            if (key == "Content-Length") {
                try {
                    contentLength = std::stoull(std::string(val));
                } catch(...) {}
            }
        }
        lineStart = lineEnd + 2;
    }

    size_t totalLen = (headersEnd - begin) + 4 + contentLength;
    if (buf->readableBytes() < totalLen) {
        return 0; // Body incomplete
    }

    if (contentLength > 0) {
        out_request.body = std::string(headersEnd + 4, contentLength);
    }

    return totalLen;
}

std::string RtspCodec::buildResponse(const RtspResponse& response) {
    std::ostringstream stream;
    stream << response.version << " " << response.status_code << " " << response.status_text << "\r\n";
    
    if (response.cseq != -1) {
        stream << "CSeq: " << response.cseq << "\r\n";
    }

    for (const auto& pair : response.headers) {
        stream << pair.first << ": " << pair.second << "\r\n";
    }

    if (!response.body.empty()) {
        stream << "Content-Length: " << response.body.size() << "\r\n";
    }
    
    stream << "\r\n";
    stream << response.body;
    
    return stream.str();
}

} // namespace protocols

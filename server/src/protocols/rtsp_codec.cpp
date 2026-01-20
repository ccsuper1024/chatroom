#include "protocols/rtsp_codec.h"
#include <sstream>
#include <algorithm>

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

size_t RtspCodec::parseRequest(const std::string& buffer, RtspRequest& out_request) {
    size_t header_end = buffer.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return 0; // Incomplete
    }

    std::string header_part = buffer.substr(0, header_end);
    std::istringstream stream(header_part);
    std::string line;

    // Request line
    if (std::getline(stream, line) && !line.empty()) {
        if (line.back() == '\r') line.pop_back();
        std::istringstream line_stream(line);
        std::string method_str;
        line_stream >> method_str >> out_request.url >> out_request.version;
        out_request.method = parseMethod(method_str);
    }

    // Headers
    while (std::getline(stream, line) && !line.empty()) {
        if (line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim spaces
            value.erase(0, value.find_first_not_of(" \t"));
            out_request.headers[key] = value;
            
            if (key == "CSeq") {
                out_request.cseq = std::stoi(value);
            }
        }
    }

    size_t body_start = header_end + 4;
    size_t content_length = 0;
    if (out_request.headers.count("Content-Length")) {
        content_length = std::stoi(out_request.headers["Content-Length"]);
    }

    if (buffer.size() < body_start + content_length) {
        return 0; // Body incomplete
    }

    out_request.body = buffer.substr(body_start, content_length);
    return body_start + content_length;
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

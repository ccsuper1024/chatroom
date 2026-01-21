#include "http/http_codec.h"

#include <sstream>
#include <string>

HttpRequest parseRequestFromBuffer(std::string& buffer, bool& complete, bool& bad_request) {
    complete = false;
    bad_request = false;
    HttpRequest request;
    std::size_t header_end = buffer.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return request;
    }
    std::string header = buffer.substr(0, header_end + 4);

    std::istringstream stream(header);
    std::string line;
    if (!std::getline(stream, line)) {
        bad_request = true;
        return request;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    {
        std::istringstream line_stream(line);
        line_stream >> request.method >> request.path;
        if (request.method.empty() || request.path.empty()) {
            bad_request = true;
            return request;
        }
    }

    std::size_t content_length = 0;
    while (std::getline(stream, line) && line != "\r") {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            while (!val.empty() && val[0] == ' ') val.erase(0, 1);
            
            request.headers[key] = val;
            
            if (key == "Content-Type") {
                request.content_type = val;
            } else if (key == "Content-Length") {
                try {
                    content_length = std::stoull(val);
                } catch (...) {
                    bad_request = true;
                    return request;
                }
            }
        }
    }

    std::size_t total_needed = header_end + 4 + content_length;
    if (buffer.size() < total_needed) {
        return request;
    }
    if (content_length > 0) {
        request.body = buffer.substr(header_end + 4, content_length);
    }
    buffer.erase(0, total_needed);
    complete = true;
    return request;
}

std::string buildResponse(const HttpResponse& response) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << response.status_code << " " << response.status_text << "\r\n";
    oss << "Content-Type: " << response.content_type << "\r\n";
    oss << "Content-Length: " << response.body.size() << "\r\n";
    oss << "Connection: keep-alive\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    for (const auto& kv : response.headers) {
        oss << kv.first << ": " << kv.second << "\r\n";
    }
    oss << "\r\n";
    oss << response.body;
    return oss.str();
}


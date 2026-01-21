#include "http/http_codec.h"
#include "net/buffer.h"

#include <sstream>
#include <string>
#include <string_view>
#include <algorithm>
#include <iostream>

HttpRequest parseRequestFromBuffer(Buffer* buf, bool& complete, bool& bad_request) {
    complete = false;
    bad_request = false;
    HttpRequest request;

    const char* begin = buf->peek();
    const char* end = buf->beginWrite();
    
    // Find double CRLF (\r\n\r\n) indicating end of headers
    const char crlf2[] = "\r\n\r\n";
    const char* headersEnd = std::search(begin, end, crlf2, crlf2 + 4);
    
    if (headersEnd == end) {
        return request; // Incomplete
    }
    
    // Parse headers
    // Using string_view for efficient parsing without copy
    std::string_view headerPart(begin, headersEnd - begin);
    
    size_t lineStart = 0;
    size_t lineEnd = headerPart.find("\r\n");
    
    // Request Line
    if (lineEnd == std::string_view::npos) {
        bad_request = true;
        return request;
    }
    
    std::string_view requestLine = headerPart.substr(0, lineEnd);
    
    // Method
    size_t methodEnd = requestLine.find(' ');
    if (methodEnd == std::string_view::npos) {
        bad_request = true;
        return request;
    }
    request.method = std::string(requestLine.substr(0, methodEnd));
    
    // Path
    size_t pathStart = methodEnd + 1;
    while (pathStart < requestLine.size() && requestLine[pathStart] == ' ') pathStart++;
    size_t pathEnd = requestLine.find(' ', pathStart);
    if (pathEnd == std::string_view::npos) {
        // HTTP/0.9 or missing version, take the rest as path
        request.path = std::string(requestLine.substr(pathStart));
    } else {
        request.path = std::string(requestLine.substr(pathStart, pathEnd - pathStart));
    }
    
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
            std::string_view valView = line.substr(colon + 1);
            // trim spaces
            while (!valView.empty() && valView.front() == ' ') valView.remove_prefix(1);
            while (!valView.empty() && valView.back() == ' ') valView.remove_suffix(1);
            
            std::string val(valView);
            request.headers[key] = val;
            
            if (key == "Content-Type") {
                request.content_type = val;
            } else if (key == "Content-Length") {
                try {
                    contentLength = std::stoull(val);
                } catch (...) {
                    bad_request = true;
                    return request;
                }
            }
        }
        
        lineStart = lineEnd + 2;
    }
    
    // Check body
    size_t headerLen = (headersEnd - begin) + 4;
    size_t totalLen = headerLen + contentLength;
    
    if (buf->readableBytes() < totalLen) {
        return request;
    }
    
    if (contentLength > 0) {
        request.body = std::string(headersEnd + 4, contentLength);
    }
    
    buf->retrieve(totalLen);
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

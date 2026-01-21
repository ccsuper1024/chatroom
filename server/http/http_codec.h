#pragma once

#include <string>
#include <map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::string content_type;
    std::string remote_ip;
    std::map<std::string, std::string> headers;
};

struct HttpResponse {
    int status_code = 200;
    std::string status_text = "OK";
    std::string body;
    std::string content_type = "application/json";
    std::map<std::string, std::string> headers;
};

HttpRequest parseRequestFromBuffer(std::string& buffer, bool& complete, bool& bad_request);

std::string buildResponse(const HttpResponse& response);


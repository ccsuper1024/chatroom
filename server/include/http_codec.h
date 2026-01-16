#pragma once

#include <string>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::string content_type;
};

struct HttpResponse {
    int status_code = 200;
    std::string status_text = "OK";
    std::string body;
    std::string content_type = "application/json";
};

HttpRequest parseRequestFromBuffer(std::string& buffer, bool& complete, bool& bad_request);

std::string buildResponse(const HttpResponse& response);


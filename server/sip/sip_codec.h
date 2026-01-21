#pragma once

#include <string>
#include <unordered_map>
#include <vector>

enum class SipMethod {
    REGISTER,
    INVITE,
    ACK,
    BYE,
    CANCEL,
    OPTIONS,
    UNKNOWN
};

struct SipRequest {
    SipMethod method;
    std::string uri;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

class SipCodec {
public:
    static size_t parseRequest(const std::string& data, SipRequest& req);
    static std::string buildResponse(int status_code, const std::string& status_text, const SipRequest& req);
    static SipMethod stringToMethod(const std::string& method);
    static std::string methodToString(SipMethod method);
};

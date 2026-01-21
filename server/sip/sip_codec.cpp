#include "sip/sip_codec.h"
#include <sstream>
#include <algorithm>

size_t SipCodec::parseRequest(const std::string& data, SipRequest& req) {
    size_t header_end = data.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return 0;
    }

    std::stringstream ss(data.substr(0, header_end));
    std::string line;
    
    // Request line
    if (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::stringstream ls(line);
        std::string method, uri, version;
        ls >> method >> uri >> version;
        req.method = stringToMethod(method);
        req.uri = uri;
        req.version = version;
    }

    // Headers
    size_t content_length = 0;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" \t"));
            req.headers[key] = value;
            
            if (key == "Content-Length" || key == "content-length") {
                try {
                    content_length = std::stoul(value);
                } catch (...) {
                    content_length = 0;
                }
            }
        }
    }

    // Body (if any)
    if (content_length > 0) {
        if (data.size() < header_end + 4 + content_length) {
            return 0; // Not enough data
        }
        req.body = data.substr(header_end + 4, content_length);
        return header_end + 4 + content_length;
    }

    return header_end + 4;
}

std::string SipCodec::buildResponse(int status_code, const std::string& status_text, const SipRequest& req) {
    std::stringstream ss;
    ss << "SIP/2.0 " << status_code << " " << status_text << "\r\n";
    // Copy some headers from request if needed (like CSeq, Call-ID, Via)
    if (req.headers.count("Via")) ss << "Via: " << req.headers.at("Via") << "\r\n";
    if (req.headers.count("From")) ss << "From: " << req.headers.at("From") << "\r\n";
    if (req.headers.count("To")) ss << "To: " << req.headers.at("To") << "\r\n";
    if (req.headers.count("Call-ID")) ss << "Call-ID: " << req.headers.at("Call-ID") << "\r\n";
    if (req.headers.count("CSeq")) ss << "CSeq: " << req.headers.at("CSeq") << "\r\n";
    ss << "Content-Length: 0\r\n";
    ss << "\r\n";
    return ss.str();
}

SipMethod SipCodec::stringToMethod(const std::string& method) {
    if (method == "REGISTER") return SipMethod::REGISTER;
    if (method == "INVITE") return SipMethod::INVITE;
    if (method == "ACK") return SipMethod::ACK;
    if (method == "BYE") return SipMethod::BYE;
    if (method == "CANCEL") return SipMethod::CANCEL;
    if (method == "OPTIONS") return SipMethod::OPTIONS;
    return SipMethod::UNKNOWN;
}

std::string SipCodec::methodToString(SipMethod method) {
    switch (method) {
        case SipMethod::REGISTER: return "REGISTER";
        case SipMethod::INVITE: return "INVITE";
        case SipMethod::ACK: return "ACK";
        case SipMethod::BYE: return "BYE";
        case SipMethod::CANCEL: return "CANCEL";
        case SipMethod::OPTIONS: return "OPTIONS";
        default: return "UNKNOWN";
    }
}

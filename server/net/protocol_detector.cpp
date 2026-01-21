#include "net/protocol_detector.h"

ProtocolType ProtocolDetector::detect(const std::string& buffer) {
    if (buffer.find("RTSP/1.0") != std::string::npos) {
        return ProtocolType::RTSP;
    } else if (buffer.find("SIP/2.0") != std::string::npos) {
        return ProtocolType::SIP;
    } else if (buffer.find("USER ") == 0) { // Simple FTP detection
        return ProtocolType::FTP;
    } else if (buffer.find("HTTP/1.1") != std::string::npos || 
               buffer.find("GET ") == 0 || 
               buffer.find("POST ") == 0) {
        return ProtocolType::HTTP;
    }
    
    // Default to HTTP for backward compatibility if not sure, 
    // or UNKNOWN if we want to be strict.
    // For now, consistent with previous logic, we default to HTTP 
    // but only if it looks somewhat like HTTP or if we accept everything else as HTTP.
    // The previous logic was: if not others, then HTTP.
    return ProtocolType::HTTP;
}

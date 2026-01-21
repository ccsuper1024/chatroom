#pragma once

#include <cstddef>

enum class ProtocolType {
    HTTP,
    RTSP,
    SIP,
    FTP,
    UNKNOWN
};

class IProtocolSession {
public:
    virtual ~IProtocolSession() = default;
    virtual ProtocolType type() const = 0;
    virtual void onData(const char* data, std::size_t len) = 0;
};


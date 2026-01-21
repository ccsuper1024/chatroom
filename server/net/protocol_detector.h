#pragma once

#include "net/protocol_session.h"
#include <string>

class ProtocolDetector {
public:
    static ProtocolType detect(const std::string& buffer);
};

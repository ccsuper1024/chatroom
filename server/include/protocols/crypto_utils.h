#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace protocols {

class CryptoUtils {
public:
    static std::string base64Encode(const std::vector<unsigned char>& data);
    static std::string base64Encode(const std::string& data);
    static std::vector<unsigned char> sha1(const std::string& data);
};

} // namespace protocols

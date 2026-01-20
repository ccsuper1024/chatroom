#pragma once

#include <string>
#include <nlohmann/json.hpp>

struct ChatMessage {
    long long id = 0;
    std::string username;
    std::string content;
    std::string timestamp;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ChatMessage, id, username, content, timestamp)
};

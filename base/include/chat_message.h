#pragma once

#include <string>
#include <nlohmann/json.hpp>

struct ChatMessage {
    long long id = 0;
    std::string username;
    std::string content;
    std::string timestamp;
    std::string target_user; // For private chat (empty if public)
    std::string room_id;     // For multi-room (empty if global)

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ChatMessage, id, username, content, timestamp, target_user, room_id)
};

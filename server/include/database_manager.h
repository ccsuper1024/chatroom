#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "chat_message.h"

// Forward declaration for sqlite3
struct sqlite3;

class DatabaseManager {
public:
    static DatabaseManager& instance();

    // Initialize database (create table if not exists)
    bool init(const std::string& db_path);

    // Add a new message
    bool addMessage(const ChatMessage& msg);

    // Get message history (limit count)
    std::vector<ChatMessage> getHistory(int limit);

    // Get messages after a specific ID
    std::vector<ChatMessage> getMessagesAfter(long long last_id);

    // Get total message count
    long long getMessageCount();

private:
    DatabaseManager();
    ~DatabaseManager();
    
    // Prevent copy
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    sqlite3* db_;
    std::mutex mutex_;
    bool initialized_;
};

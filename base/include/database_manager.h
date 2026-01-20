#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include "database.h"
#include "database_config.h"

class DatabaseManager {
public:
    static DatabaseManager& instance();

    // Initialize database
    bool init(const DatabaseConfig& config);

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

    std::unique_ptr<Database> db_;
    std::mutex mutex_;
};

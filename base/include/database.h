#pragma once
#include <string>
#include <vector>
#include "chat_message.h"
#include "database_config.h"

class Database {
public:
    virtual ~Database() = default;
    
    // Initialize the database connection
    virtual bool init(const DatabaseConfig& config) = 0;
    
    // Add a new message
    virtual bool addMessage(const ChatMessage& msg) = 0;
    
    // Get message history (limit count)
    virtual std::vector<ChatMessage> getHistory(int limit) = 0;
    
    // Get messages after a specific ID
    virtual std::vector<ChatMessage> getMessagesAfter(long long last_id) = 0;
    
    // Get total message count
    virtual long long getMessageCount() = 0;
};

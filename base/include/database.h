#pragma once
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
    
    // Get message history (limit count, optionally filter for a user)
    virtual std::vector<ChatMessage> getHistory(int limit, const std::string& username = "") = 0;
    
    // Get messages after a specific ID (optionally filter for a user)
    virtual std::vector<ChatMessage> getMessagesAfter(long long last_id, const std::string& username = "") = 0;
    
    // Get total message count
    virtual long long getMessageCount() = 0;

    // User Management
    virtual bool addUser(const std::string& username, const std::string& password) = 0;
    virtual bool validateUser(const std::string& username, const std::string& password) = 0;
    virtual bool userExists(const std::string& username) = 0;
    virtual long long getUserId(const std::string& username) = 0;
};

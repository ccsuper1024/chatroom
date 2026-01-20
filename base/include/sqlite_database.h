#pragma once
#include "database.h"
#include <mutex>

// Forward declaration
struct sqlite3;

class SqliteDatabase : public Database {
public:
    SqliteDatabase();
    ~SqliteDatabase() override;

    bool init(const DatabaseConfig& config) override;
    bool addMessage(const ChatMessage& msg) override;
    std::vector<ChatMessage> getHistory(int limit, const std::string& username = "") override;
    std::vector<ChatMessage> getMessagesAfter(long long last_id, const std::string& username = "") override;
    long long getMessageCount() override;

private:
    sqlite3* db_;
    std::mutex mutex_;
    bool initialized_;
};

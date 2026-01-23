#pragma once
#include "database.h"
#include <mutex>
#include <mysql/mysql.h>
#include <queue>
#include <condition_variable>

class MysqlDatabase : public Database {
public:
    MysqlDatabase();
    ~MysqlDatabase() override;

    bool init(const DatabaseConfig& config) override;
    bool addMessage(const ChatMessage& msg) override;
    std::vector<ChatMessage> getHistory(int limit, const std::string& username = "") override;
    std::vector<ChatMessage> getMessagesAfter(long long last_id, const std::string& username = "") override;
    long long getMessageCount() override;

    bool addUser(const std::string& username, const std::string& password) override;
    bool validateUser(const std::string& username, const std::string& password) override;
    bool userExists(const std::string& username) override;
    long long getUserId(const std::string& username) override;
    std::vector<std::pair<std::string, long long>> getAllUsers() override;
    
private: friend class ConnectionGuard;

private:
    DatabaseConfig config_;
    std::queue<MYSQL*> connection_pool_;
    std::mutex pool_mutex_;
    std::condition_variable pool_cv_;
    int current_pool_size_;
    bool initialized_;

    MYSQL* getConnection();
    void releaseConnection(MYSQL* conn);
    MYSQL* createConnection();
};

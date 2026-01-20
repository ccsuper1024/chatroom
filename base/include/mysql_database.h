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
    std::vector<ChatMessage> getHistory(int limit) override;
    std::vector<ChatMessage> getMessagesAfter(long long last_id) override;
    long long getMessageCount() override;

private:
    std::queue<MYSQL*> connection_pool_;
    std::mutex pool_mutex_;
    std::condition_variable pool_cv_;
    int current_pool_size_;
    bool initialized_;
    DatabaseConfig config_;

    MYSQL* getConnection();
    void releaseConnection(MYSQL* conn);
    MYSQL* createConnection();
};

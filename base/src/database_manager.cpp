#include "database_manager.h"
#include "sqlite_database.h"
#include "mysql_database.h"
#include "logger.h"

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager() {}
DatabaseManager::~DatabaseManager() {}

bool DatabaseManager::init(const DatabaseConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (config.type == "mysql") {
        db_ = std::make_unique<MysqlDatabase>();
    } else {
        db_ = std::make_unique<SqliteDatabase>();
    }
    
    return db_->init(config);
}

bool DatabaseManager::addMessage(const ChatMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;
    return db_->addMessage(msg);
}

std::vector<ChatMessage> DatabaseManager::getHistory(int limit, const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return {};
    return db_->getHistory(limit, username);
}

std::vector<ChatMessage> DatabaseManager::getMessagesAfter(long long last_id, const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return {};
    return db_->getMessagesAfter(last_id, username);
}

long long DatabaseManager::getMessageCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return 0;
    return db_->getMessageCount();
}

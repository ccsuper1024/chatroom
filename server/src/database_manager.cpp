#include "database_manager.h"
#include "logger.h"
#include <sqlite3.h>
#include <iostream>

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager() : db_(nullptr), initialized_(false) {}

DatabaseManager::~DatabaseManager() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool DatabaseManager::init(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;

    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc) {
        LOG_ERROR("Can't open database: {}", sqlite3_errmsg(db_));
        return false;
    }

    const char* sql = "CREATE TABLE IF NOT EXISTS messages ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "username TEXT NOT NULL,"
                      "content TEXT NOT NULL,"
                      "timestamp TEXT NOT NULL);";

    char* zErrMsg = 0;
    rc = sqlite3_exec(db_, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        LOG_ERROR("SQL error: {}", zErrMsg);
        sqlite3_free(zErrMsg);
        return false;
    }

    initialized_ = true;
    LOG_INFO("Database initialized successfully at {}", db_path);
    return true;
}

bool DatabaseManager::addMessage(const ChatMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !db_) return false;

    const char* sql = "INSERT INTO messages (username, content, timestamp) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, msg.username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, msg.content.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, msg.timestamp.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        LOG_ERROR("Execution failed: {}", sqlite3_errmsg(db_));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

std::vector<ChatMessage> DatabaseManager::getHistory(int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ChatMessage> history;
    if (!initialized_ || !db_) return history;

    // Get last N messages in chronological order
    std::string sql = "SELECT id, username, content, timestamp FROM ("
                      "SELECT * FROM messages ORDER BY id DESC LIMIT ?) "
                      "ORDER BY id ASC;";
                      
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return history;
    }

    sqlite3_bind_int(stmt, 1, limit);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        msg.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        history.push_back(msg);
    }

    sqlite3_finalize(stmt);
    return history;
}

long long DatabaseManager::getMessageCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !db_) return 0;
    
    const char* sql = "SELECT COUNT(*) FROM messages;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) return 0;
    
    long long count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

std::vector<ChatMessage> DatabaseManager::getMessagesAfter(long long last_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ChatMessage> history;
    if (!initialized_ || !db_) return history;

    std::string sql = "SELECT id, username, content, timestamp FROM messages WHERE id > ? ORDER BY id ASC;";
                      
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return history;
    }

    sqlite3_bind_int64(stmt, 1, last_id);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        msg.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        history.push_back(msg);
    }

    sqlite3_finalize(stmt);
    return history;
}

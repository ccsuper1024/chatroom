#include "sqlite_database.h"
#include "logger.h"
#include <sqlite3.h>
#include <iostream>

SqliteDatabase::SqliteDatabase() : db_(nullptr), initialized_(false) {}

SqliteDatabase::~SqliteDatabase() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool SqliteDatabase::init(const DatabaseConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;

    std::string db_path = config.path;
    if (db_path.empty()) db_path = "chatroom.db";

    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc) {
        LOG_ERROR("Can't open database: {}", sqlite3_errmsg(db_));
        return false;
    }

    const char* sql = "CREATE TABLE IF NOT EXISTS messages ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                      "username TEXT NOT NULL,"
                      "content TEXT NOT NULL,"
                      "timestamp TEXT NOT NULL,"
                      "target_user TEXT,"
                      "room_id TEXT"
                      ");";

    char* zErrMsg = 0;
    rc = sqlite3_exec(db_, sql, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        LOG_ERROR("SQL error: {}", zErrMsg);
        sqlite3_free(zErrMsg);
        return false;
    }

    // Attempt to add columns if they don't exist (migration for existing DB)
    const char* alter_sql1 = "ALTER TABLE messages ADD COLUMN target_user TEXT;";
    sqlite3_exec(db_, alter_sql1, 0, 0, 0); // Ignore error if exists

    const char* alter_sql2 = "ALTER TABLE messages ADD COLUMN room_id TEXT;";
    sqlite3_exec(db_, alter_sql2, 0, 0, 0); // Ignore error if exists

    initialized_ = true;
    LOG_INFO("SQLite Database initialized successfully at {}", db_path);
    return true;
}

bool SqliteDatabase::addMessage(const ChatMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !db_) return false;

    const char* sql = "INSERT INTO messages (username, content, timestamp, target_user, room_id) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return false;
    }

    sqlite3_bind_text(stmt, 1, msg.username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, msg.content.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, msg.timestamp.c_str(), -1, SQLITE_STATIC);
    
    if (msg.target_user.empty()) {
        sqlite3_bind_null(stmt, 4);
    } else {
        sqlite3_bind_text(stmt, 4, msg.target_user.c_str(), -1, SQLITE_STATIC);
    }

    if (msg.room_id.empty()) {
        sqlite3_bind_null(stmt, 5);
    } else {
        sqlite3_bind_text(stmt, 5, msg.room_id.c_str(), -1, SQLITE_STATIC);
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        LOG_ERROR("Execution failed: {}", sqlite3_errmsg(db_));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

std::vector<ChatMessage> SqliteDatabase::getHistory(int limit, const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ChatMessage> history;
    if (!initialized_ || !db_) return history;

    std::string sql;
    sqlite3_stmt* stmt;
    int rc;

    if (username.empty()) {
        // Only public messages
        sql = "SELECT id, username, content, timestamp, target_user, room_id FROM ("
              "SELECT * FROM messages WHERE (target_user IS NULL OR target_user = '') ORDER BY id DESC LIMIT ?) "
              "ORDER BY id ASC;";
        rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, 0);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, limit);
        }
    } else {
        // Public + Private for user + Sent by user
        sql = "SELECT id, username, content, timestamp, target_user, room_id FROM ("
              "SELECT * FROM messages WHERE (target_user IS NULL OR target_user = '' OR target_user = ? OR username = ?) ORDER BY id DESC LIMIT ?) "
              "ORDER BY id ASC;";
        rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, 0);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 3, limit);
        }
    }

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return history;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        msg.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        
        const char* target = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        msg.target_user = target ? target : "";
        
        const char* room = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        msg.room_id = room ? room : "";
        
        history.push_back(msg);
    }

    sqlite3_finalize(stmt);
    return history;
}

std::vector<ChatMessage> SqliteDatabase::getMessagesAfter(long long last_id, const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ChatMessage> history;
    if (!initialized_ || !db_) return history;

    std::string sql;
    sqlite3_stmt* stmt;
    int rc;

    if (username.empty()) {
        // Only public messages
        sql = "SELECT id, username, content, timestamp, target_user, room_id FROM messages "
              "WHERE id > ? AND (target_user IS NULL OR target_user = '') ORDER BY id ASC;";
        rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, 0);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, last_id);
        }
    } else {
        // Public + Private for user + Sent by user
        sql = "SELECT id, username, content, timestamp, target_user, room_id FROM messages "
              "WHERE id > ? AND (target_user IS NULL OR target_user = '' OR target_user = ? OR username = ?) ORDER BY id ASC;";
        rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, 0);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, last_id);
            sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_STATIC);
        }
    }

    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: {}", sqlite3_errmsg(db_));
        return history;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        ChatMessage msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        msg.content = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        msg.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        
        const char* target = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        msg.target_user = target ? target : "";
        
        const char* room = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        msg.room_id = room ? room : "";
        
        history.push_back(msg);
    }

    sqlite3_finalize(stmt);
    return history;
}

long long SqliteDatabase::getMessageCount() {
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

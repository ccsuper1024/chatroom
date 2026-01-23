#include "mysql_database.h"
#include "logger.h"
#include <sstream>

class ConnectionGuard {
public:
    ConnectionGuard(MysqlDatabase* db) : db_(db) {
        conn_ = db_->getConnection();
    }
    ~ConnectionGuard() {
        if (conn_) {
            db_->releaseConnection(conn_);
        }
    }
    MYSQL* get() { return conn_; }

private:
    MysqlDatabase* db_;
    MYSQL* conn_;
};

MysqlDatabase::MysqlDatabase() : current_pool_size_(0), initialized_(false) {
}

MysqlDatabase::~MysqlDatabase() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    while (!connection_pool_.empty()) {
        MYSQL* conn = connection_pool_.front();
        connection_pool_.pop();
        mysql_close(conn);
    }
}

MYSQL* MysqlDatabase::createConnection() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        LOG_ERROR("MySQL init failed");
        return nullptr;
    }

    // Set timeout options
    unsigned int timeout = 5;
    mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    bool reconnect = true;
    mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(conn, config_.host.c_str(), config_.user.c_str(), 
                           config_.password.c_str(), config_.db_name.c_str(), 
                           config_.port, nullptr, 0)) {
        LOG_ERROR("MySQL connection failed: {}", mysql_error(conn));
        mysql_close(conn);
        return nullptr;
    }
    return conn;
}

bool MysqlDatabase::init(const DatabaseConfig& config) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    if (initialized_) return true;
    
    config_ = config;
    
    // Create initial connections
    for (int i = 0; i < config.initial_size; ++i) {
        MYSQL* conn = createConnection();
        if (conn) {
            connection_pool_.push(conn);
            current_pool_size_++;
        } else {
            LOG_ERROR("Failed to create initial connection {}", i);
        }
    }

    if (connection_pool_.empty()) {
        return false;
    }

    // Use one connection to create table
    MYSQL* conn = connection_pool_.front();
    
    // Create table if not exists
    const char* sql = "CREATE TABLE IF NOT EXISTS messages ("
                      "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
                      "username VARCHAR(255) NOT NULL,"
                      "content TEXT NOT NULL,"
                      "timestamp VARCHAR(64) NOT NULL,"
                      "target_user VARCHAR(255),"
                      "room_id VARCHAR(255)"
                      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
                      
    if (mysql_query(conn, sql)) {
        LOG_ERROR("MySQL create table error: {}", mysql_error(conn));
        return false;
    }

    const char* user_sql = "CREATE TABLE IF NOT EXISTS users ("
                           "id BIGINT PRIMARY KEY AUTO_INCREMENT,"
                           "username VARCHAR(255) NOT NULL UNIQUE,"
                           "password VARCHAR(255) NOT NULL"
                           ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
    
    if (mysql_query(conn, user_sql)) {
        LOG_ERROR("MySQL create users table error: {}", mysql_error(conn));
        return false;
    }

    // Attempt to add columns if they don't exist (migration for existing DB)
    // We ignore errors because if column exists, it will fail, which is fine.
    const char* alter_sql1 = "ALTER TABLE messages ADD COLUMN target_user VARCHAR(255);";
    mysql_query(conn, alter_sql1); 

    const char* alter_sql2 = "ALTER TABLE messages ADD COLUMN room_id VARCHAR(255);";
    mysql_query(conn, alter_sql2); 

    initialized_ = true;
    LOG_INFO("MySQL Database initialized successfully with {} connections", current_pool_size_);
    return true;
}

MYSQL* MysqlDatabase::getConnection() {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    while (connection_pool_.empty()) {
        if (current_pool_size_ < config_.max_size) {
            MYSQL* conn = createConnection();
            if (conn) {
                current_pool_size_++;
                return conn;
            }
            // Failed to create, wait for others
        }
        pool_cv_.wait(lock);
    }
    
    MYSQL* conn = connection_pool_.front();
    connection_pool_.pop();
    
    // Check connection health
    if (mysql_ping(conn)) {
        LOG_WARN("MySQL connection lost, attempting reconnect...");
        mysql_close(conn);
        conn = createConnection();
        if (!conn) {
            current_pool_size_--;
            return nullptr;
        }
    }
    
    return conn;
}

void MysqlDatabase::releaseConnection(MYSQL* conn) {
    if (!conn) return;
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    connection_pool_.push(conn);
    pool_cv_.notify_one();
}

bool MysqlDatabase::addMessage(const ChatMessage& msg) {
    if (!initialized_) return false;
    
    MYSQL* conn = getConnection();
    if (!conn) return false;

    char* escaped_username = new char[msg.username.length() * 2 + 1];
    char* escaped_content = new char[msg.content.length() * 2 + 1];
    char* escaped_timestamp = new char[msg.timestamp.length() * 2 + 1];
    
    mysql_real_escape_string(conn, escaped_username, msg.username.c_str(), msg.username.length());
    mysql_real_escape_string(conn, escaped_content, msg.content.c_str(), msg.content.length());
    mysql_real_escape_string(conn, escaped_timestamp, msg.timestamp.c_str(), msg.timestamp.length());

    std::string target_val = "NULL";
    if (!msg.target_user.empty()) {
        char* escaped_target = new char[msg.target_user.length() * 2 + 1];
        mysql_real_escape_string(conn, escaped_target, msg.target_user.c_str(), msg.target_user.length());
        target_val = "'" + std::string(escaped_target) + "'";
        delete[] escaped_target;
    }

    std::string room_val = "NULL";
    if (!msg.room_id.empty()) {
        char* escaped_room = new char[msg.room_id.length() * 2 + 1];
        mysql_real_escape_string(conn, escaped_room, msg.room_id.c_str(), msg.room_id.length());
        room_val = "'" + std::string(escaped_room) + "'";
        delete[] escaped_room;
    }

    std::stringstream ss;
    ss << "INSERT INTO messages (username, content, timestamp, target_user, room_id) VALUES ('"
       << escaped_username << "', '"
       << escaped_content << "', '"
       << escaped_timestamp << "', "
       << target_val << ", "
       << room_val << ")";
    
    std::string sql = ss.str();
    
    delete[] escaped_username;
    delete[] escaped_content;
    delete[] escaped_timestamp;

    bool success = true;
    if (mysql_query(conn, sql.c_str())) {
        LOG_ERROR("MySQL insert error: {}", mysql_error(conn));
        success = false;
    }
    
    releaseConnection(conn);
    return success;
}

std::vector<ChatMessage> MysqlDatabase::getHistory(int limit, const std::string& username) {
    std::vector<ChatMessage> history;
    if (!initialized_) return history;
    
    MYSQL* conn = getConnection();
    if (!conn) return history;

    std::string where_clause;
    if (username.empty()) {
        where_clause = " WHERE (target_user IS NULL OR target_user = '')";
    } else {
        char* escaped_user = new char[username.length() * 2 + 1];
        mysql_real_escape_string(conn, escaped_user, username.c_str(), username.length());
        std::string u(escaped_user);
        delete[] escaped_user;
        
        where_clause = " WHERE (target_user IS NULL OR target_user = '' OR target_user = '" + u + "' OR username = '" + u + "')";
    }

    // Use specific columns to ensure order match
    std::string sql = "SELECT id, username, content, timestamp, target_user, room_id FROM ("
                      "SELECT * FROM messages" + where_clause + " ORDER BY id DESC LIMIT " + std::to_string(limit) + ") "
                      "AS sub ORDER BY id ASC";

    if (mysql_query(conn, sql.c_str())) {
        LOG_ERROR("MySQL query error: {}", mysql_error(conn));
        releaseConnection(conn);
        return history;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        LOG_ERROR("MySQL store result error: {}", mysql_error(conn));
        releaseConnection(conn);
        return history;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        ChatMessage msg;
        msg.id = std::stoll(row[0]);
        msg.username = row[1] ? row[1] : "";
        msg.content = row[2] ? row[2] : "";
        msg.timestamp = row[3] ? row[3] : "";
        msg.target_user = row[4] ? row[4] : "";
        msg.room_id = row[5] ? row[5] : "";
        history.push_back(msg);
    }
    
    mysql_free_result(res);
    releaseConnection(conn);
    return history;
}

std::vector<ChatMessage> MysqlDatabase::getMessagesAfter(long long last_id, const std::string& username) {
    std::vector<ChatMessage> history;
    if (!initialized_) return history;
    
    MYSQL* conn = getConnection();
    if (!conn) return history;

    std::string where_clause;
    if (username.empty()) {
        where_clause = " AND (target_user IS NULL OR target_user = '')";
    } else {
        char* escaped_user = new char[username.length() * 2 + 1];
        mysql_real_escape_string(conn, escaped_user, username.c_str(), username.length());
        std::string u(escaped_user);
        delete[] escaped_user;
        
        where_clause = " AND (target_user IS NULL OR target_user = '' OR target_user = '" + u + "' OR username = '" + u + "')";
    }

    std::string sql = "SELECT id, username, content, timestamp, target_user, room_id FROM messages WHERE id > " + 
                      std::to_string(last_id) + where_clause + " ORDER BY id ASC";

    if (mysql_query(conn, sql.c_str())) {
        LOG_ERROR("MySQL query error: {}", mysql_error(conn));
        releaseConnection(conn);
        return history;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        LOG_ERROR("MySQL store result error: {}", mysql_error(conn));
        releaseConnection(conn);
        return history;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        ChatMessage msg;
        msg.id = std::stoll(row[0]);
        msg.username = row[1] ? row[1] : "";
        msg.content = row[2] ? row[2] : "";
        msg.timestamp = row[3] ? row[3] : "";
        msg.target_user = row[4] ? row[4] : "";
        msg.room_id = row[5] ? row[5] : "";
        history.push_back(msg);
    }
    
    mysql_free_result(res);
    releaseConnection(conn);
    return history;
}

long long MysqlDatabase::getMessageCount() {
    if (!initialized_) return 0;
    
    MYSQL* conn = getConnection();
    if (!conn) return 0;
    
    if (mysql_query(conn, "SELECT COUNT(*) FROM messages")) {
        releaseConnection(conn);
        return 0;
    }
    
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        releaseConnection(conn);
        return 0;
    }
    
    long long count = 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row && row[0]) {
        count = std::stoll(row[0]);
    }
    
    mysql_free_result(res);
    releaseConnection(conn);
    return count;
}

bool MysqlDatabase::addUser(const std::string& username, const std::string& password) {
    if (!initialized_) return false;
    
    MYSQL* conn = getConnection();
    if (!conn) return false;

    char* escaped_username = new char[username.length() * 2 + 1];
    char* escaped_password = new char[password.length() * 2 + 1];
    
    mysql_real_escape_string(conn, escaped_username, username.c_str(), username.length());
    mysql_real_escape_string(conn, escaped_password, password.c_str(), password.length());
    
    std::string sql = "INSERT INTO users (username, password) VALUES ('" + 
                      std::string(escaped_username) + "', '" + 
                      std::string(escaped_password) + "')";
                      
    delete[] escaped_username;
    delete[] escaped_password;
    
    bool success = true;
    if (mysql_query(conn, sql.c_str())) {
        LOG_ERROR("MySQL insert user error: {}", mysql_error(conn));
        success = false;
    }
    
    releaseConnection(conn);
    return success;
}

bool MysqlDatabase::validateUser(const std::string& username, const std::string& password) {
    if (!initialized_) return false;
    
    MYSQL* conn = getConnection();
    if (!conn) return false;

    char* escaped_username = new char[username.length() * 2 + 1];
    mysql_real_escape_string(conn, escaped_username, username.c_str(), username.length());
    
    std::string sql = "SELECT password FROM users WHERE username = '" + std::string(escaped_username) + "'";
    delete[] escaped_username;
    
    if (mysql_query(conn, sql.c_str())) {
        LOG_ERROR("MySQL query user error: {}", mysql_error(conn));
        releaseConnection(conn);
        return false;
    }
    
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        releaseConnection(conn);
        return false;
    }
    
    bool valid = false;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row && row[0]) {
        if (password == std::string(row[0])) {
            valid = true;
        }
    }
    
    mysql_free_result(res);
    releaseConnection(conn);
    return valid;
}

bool MysqlDatabase::userExists(const std::string& username) {
    ConnectionGuard conn_guard(this);
    MYSQL* conn = conn_guard.get();
    if (!conn) return false;

    std::stringstream ss;
    char escaped_username[512];
    mysql_real_escape_string(conn, escaped_username, username.c_str(), username.length());
    
    ss << "SELECT COUNT(*) FROM users WHERE username = '" << escaped_username << "'";
    
    if (mysql_query(conn, ss.str().c_str())) {
        return false;
    }
    
    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) return false;
    
    bool exists = false;
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        exists = std::stoll(row[0]) > 0;
    }
    
    mysql_free_result(result);
    return exists;
}

long long MysqlDatabase::getUserId(const std::string& username) {
    ConnectionGuard conn_guard(this);
    MYSQL* conn = conn_guard.get();
    if (!conn) return -1;

    std::stringstream ss;
    char escaped_username[512];
    mysql_real_escape_string(conn, escaped_username, username.c_str(), username.length());
    
    ss << "SELECT id FROM users WHERE username = '" << escaped_username << "'";
    
    if (mysql_query(conn, ss.str().c_str())) {
        return -1;
    }
    
    MYSQL_RES* result = mysql_store_result(conn);
    if (!result) return -1;
    
    long long id = -1;
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        id = std::stoll(row[0]);
    }
    
    mysql_free_result(result);
    return id;
}

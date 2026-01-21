#pragma once

#include <string>
#include <mutex>
#include <vector>
#include <chrono>
#include "database_config.h"

struct LogConfig {
    std::string level = "info";
    std::string file_path = "logs/chatroom.log";
    bool console_output = true;
    std::size_t max_size = 5 * 1024 * 1024;
    std::size_t max_files = 3;
};

struct ThreadPoolConfig {
    std::size_t core_threads = 0;
    std::size_t max_threads = 0;
    std::size_t queue_capacity = 1024;
    std::size_t io_threads = 0; // 0 means loop in main thread
};

struct RateLimitConfig {
    int window_seconds = 60;
    int max_requests = 60;
    bool enabled = true;
};

struct ServerConfig {
    // Basic
    int port = 8080;
    
    // Logging
    LogConfig logging;
    
    // Thread Pool
    ThreadPoolConfig thread_pool;
    
    // Database
    DatabaseConfig db;
    
    // Connection & Heartbeat
    int connection_check_interval_seconds = 30;
    int max_connection_failures = 3;
    int heartbeat_timeout_seconds = 60;
    int session_cleanup_interval_seconds = 30;
    
    // Limits
    std::size_t max_message_history = 1000;
    std::string history_file_path = "data/chat_history.json";
    std::size_t max_message_length = 1024;
    std::size_t max_username_length = 32;
    RateLimitConfig rate_limit;

    // Singleton access
    static ServerConfig& instance();
    
    // Load from file
    bool load(const std::string& config_file);

private:
    ServerConfig() = default;
    ServerConfig(const ServerConfig&) = delete;
    ServerConfig& operator=(const ServerConfig&) = delete;
};

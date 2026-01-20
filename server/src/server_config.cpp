#include "server_config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <iostream>

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static bool parseBool(const std::string& value) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    return v == "true" || v == "1" || v == "on" || v == "yes";
}

ServerConfig& ServerConfig::instance() {
    static ServerConfig instance;
    return instance;
}

bool ServerConfig::load(const std::string& config_file) {
    std::ifstream in(config_file);
    if (!in.is_open()) {
        // If file doesn't exist, use defaults but try to calculate thread pool
        // based on hardware if they are 0
        if (thread_pool.core_threads == 0) {
             std::size_t hw = std::thread::hardware_concurrency();
             if (hw == 0) hw = 4;
             thread_pool.core_threads = std::max<std::size_t>(1, hw / 2);
             thread_pool.max_threads = std::max<std::size_t>(thread_pool.core_threads, hw * 2);
        }
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') {
            continue;
        }
        auto pos = t.find(':');
        if (pos == std::string::npos) {
            continue;
        }
        std::string key = trim(t.substr(0, pos));
        std::string value = trim(t.substr(pos + 1));
        
        try {
            if (key == "port") {
                port = std::stoi(value);
            } else if (key == "log_level") {
                logging.level = value;
            } else if (key == "log_file") {
                logging.file_path = value;
            } else if (key == "log_console") {
                logging.console_output = parseBool(value);
            } else if (key == "log_max_size") {
                logging.max_size = std::stoul(value);
            } else if (key == "log_max_files") {
                logging.max_files = std::stoul(value);
            } else if (key == "thread_pool_core") {
                thread_pool.core_threads = std::stoul(value);
            } else if (key == "thread_pool_max") {
                thread_pool.max_threads = std::stoul(value);
            } else if (key == "thread_queue_capacity") {
                thread_pool.queue_capacity = std::stoul(value);
            } else if (key == "check_interval_seconds") {
                connection_check_interval_seconds = std::stoi(value);
            } else if (key == "max_failures") {
                max_connection_failures = std::stoi(value);
            } else if (key == "heartbeat_timeout_seconds") {
                heartbeat_timeout_seconds = std::stoi(value);
            } else if (key == "session_cleanup_interval_seconds") {
                session_cleanup_interval_seconds = std::stoi(value);
            } else if (key == "max_message_history") {
                max_message_history = std::stoul(value);
            } else if (key == "max_message_length") {
                max_message_length = std::stoul(value);
            } else if (key == "rate_limit_enabled") {
                rate_limit.enabled = parseBool(value);
            } else if (key == "rate_limit_window") {
                rate_limit.window_seconds = std::stoi(value);
            } else if (key == "rate_limit_max_requests") {
                rate_limit.max_requests = std::stoi(value);
            }
        } catch (...) {
            std::cerr << "Error parsing config key: " << key << ", value: " << value << std::endl;
        }
    }

    // Post-load validation and defaults
    if (thread_pool.core_threads == 0) {
        std::size_t hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 4;
        thread_pool.core_threads = std::max<std::size_t>(1, hw / 2);
    }
    if (thread_pool.max_threads == 0) {
        // Ensure max is at least core
        thread_pool.max_threads = std::max<std::size_t>(thread_pool.core_threads, 
            std::max<std::size_t>(4, std::thread::hardware_concurrency() * 2));
    }
    if (thread_pool.queue_capacity == 0) {
        thread_pool.queue_capacity = 1024;
    }
    
    return true;
}

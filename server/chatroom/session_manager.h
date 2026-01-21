#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <vector>
#include <memory>
#include "utils/metrics_collector.h"

struct UserSession {
    std::string username;
    std::string connection_id;
    std::string client_version;
    std::chrono::system_clock::time_point last_heartbeat;
    std::chrono::system_clock::time_point login_time;
};

class SessionManager {
public:
    SessionManager(std::shared_ptr<MetricsCollector> metrics);
    ~SessionManager();

    void start();
    void stop();

    // Session Management
    struct LoginResult {
        bool success;
        std::string error_msg;
        std::string connection_id;
    };
    LoginResult login(const std::string& username);
    
    bool updateHeartbeat(const std::string& connection_id, const std::string& client_version);
    
    // Getters
    std::string getUsername(const std::string& connection_id);
    std::vector<UserSession> getAllSessions();
    
private:
    void cleanupLoop();
    std::string generateConnectionId();

    std::shared_ptr<MetricsCollector> metrics_collector_;
    std::unordered_map<std::string, UserSession> sessions_;
    std::mutex mutex_;
    
    std::atomic<bool> running_;
    std::thread cleanup_thread_;
    std::mutex cleanup_mutex_;
    std::condition_variable cleanup_cv_;
};

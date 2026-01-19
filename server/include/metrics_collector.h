#pragma once

#include <mutex>
#include <atomic>
#include <string>
#include <unordered_map>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class MetricsCollector {
public:
    MetricsCollector();

    void recordRequest(const std::string& method, const std::string& path);
    void recordError(const std::string& type);
    void updateActiveSessions(size_t count);
    void updateMessageCount(size_t count);
    
    json getMetrics() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, size_t> request_counts_;
    std::unordered_map<std::string, size_t> error_counts_;
    std::atomic<size_t> active_sessions_{0};
    std::atomic<size_t> message_count_{0};
    std::chrono::system_clock::time_point start_time_;
};

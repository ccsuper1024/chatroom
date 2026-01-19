#include "metrics_collector.h"
#include <sstream>
#include <iomanip>

MetricsCollector::MetricsCollector() {
    start_time_ = std::chrono::system_clock::now();
}

void MetricsCollector::recordRequest(const std::string& method, const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    request_counts_[method + " " + path]++;
}

void MetricsCollector::recordError(const std::string& type) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_counts_[type]++;
}

void MetricsCollector::updateActiveSessions(size_t count) {
    active_sessions_.store(count);
}

void MetricsCollector::updateMessageCount(size_t count) {
    message_count_.store(count);
}

json MetricsCollector::getMetrics() const {
    json metrics;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics["requests"] = request_counts_;
        metrics["errors"] = error_counts_;
    }
    
    metrics["active_sessions"] = active_sessions_.load();
    metrics["message_count"] = message_count_.load();
    
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    metrics["uptime_seconds"] = uptime;
    
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    metrics["timestamp"] = oss.str();
    
    return metrics;
}

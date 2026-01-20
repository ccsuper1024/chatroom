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

std::string MetricsCollector::getPrometheusMetrics() const {
    std::ostringstream ss;
    
    // Uptime
    auto now = std::chrono::system_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    ss << "# HELP chatroom_uptime_seconds Server uptime in seconds\n";
    ss << "# TYPE chatroom_uptime_seconds gauge\n";
    ss << "chatroom_uptime_seconds " << uptime << "\n";

    // Active Sessions
    ss << "# HELP chatroom_active_sessions Number of active sessions\n";
    ss << "# TYPE chatroom_active_sessions gauge\n";
    ss << "chatroom_active_sessions " << active_sessions_.load() << "\n";

    // Stored Messages
    ss << "# HELP chatroom_stored_messages Number of messages in memory\n";
    ss << "# TYPE chatroom_stored_messages gauge\n";
    ss << "chatroom_stored_messages " << message_count_.load() << "\n";

    std::lock_guard<std::mutex> lock(mutex_);

    // Requests
    ss << "# HELP chatroom_requests_total Total number of HTTP requests\n";
    ss << "# TYPE chatroom_requests_total counter\n";
    for (const auto& pair : request_counts_) {
        // Key is "METHOD PATH"
        std::string method, path;
        std::istringstream iss(pair.first);
        iss >> method >> path;
        ss << "chatroom_requests_total{method=\"" << method << "\",path=\"" << path << "\"} " << pair.second << "\n";
    }

    // Errors
    ss << "# HELP chatroom_errors_total Total number of errors\n";
    ss << "# TYPE chatroom_errors_total counter\n";
    for (const auto& pair : error_counts_) {
        ss << "chatroom_errors_total{type=\"" << pair.first << "\"} " << pair.second << "\n";
    }

    return ss.str();
}

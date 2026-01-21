#include "utils/rate_limiter.h"
#include "utils/server_config.h"
#include "logger.h"

RateLimiter::RateLimiter() {}

bool RateLimiter::check(const std::string& ip) {
    const auto& config = ServerConfig::instance().rate_limit;
    if (!config.enabled) {
        return true;
    }
    
    if (ip.empty()) {
        LOG_WARN("Rate limit check skipped for empty IP");
        return true; 
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto& entry = limits_[ip];
    
    if (entry.reset_time < now) {
        entry.count = 0;
        entry.reset_time = now + std::chrono::seconds(config.window_seconds);
    }
    
    if (entry.count >= config.max_requests) {
        LOG_WARN("IP {} rate limited", ip);
        return false;
    }
    
    entry.count++;
    return true;
}

#pragma once

#include <string>
#include <mutex>
#include <map>
#include <chrono>

class RateLimiter {
public:
    RateLimiter();
    ~RateLimiter() = default;

    // 禁止拷贝和赋值
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    bool check(const std::string& ip);

private:
    struct Entry {
        int count = 0;
        std::chrono::steady_clock::time_point reset_time;
    };

    std::mutex mutex_;
    std::map<std::string, Entry> limits_;
};

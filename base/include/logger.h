#pragma once

#include <memory>
#include <string>
#include <utility>
#include <spdlog/spdlog.h>

class Logger {
public:
    static Logger& instance();

    template <typename... Args>
    void info(const char* fmt, Args&&... args) {
        logger_->info(SPDLOG_FMT_RUNTIME(fmt), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(const char* fmt, Args&&... args) {
        logger_->warn(SPDLOG_FMT_RUNTIME(fmt), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(const char* fmt, Args&&... args) {
        logger_->error(SPDLOG_FMT_RUNTIME(fmt), std::forward<Args>(args)...);
    }

    template <typename... Args>
    void debug(const char* fmt, Args&&... args) {
        logger_->debug(SPDLOG_FMT_RUNTIME(fmt), std::forward<Args>(args)...);
    }

    void setLevel(spdlog::level::level_enum level);
    void setPattern(const std::string& pattern);

private:
    Logger();
    std::shared_ptr<spdlog::logger> logger_;
};

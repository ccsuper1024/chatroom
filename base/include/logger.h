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

    template <typename... Args>
    void logWithLocation(spdlog::level::level_enum level,
                         const spdlog::source_loc& loc,
                         const char* fmt,
                         Args&&... args) {
        logger_->log(loc, level, SPDLOG_FMT_RUNTIME(fmt), std::forward<Args>(args)...);
    }

    void setLevel(spdlog::level::level_enum level);
    void setPattern(const std::string& pattern);
    void configure(bool console, const std::string& file_path, const std::string& level);

private:
    Logger();
    std::shared_ptr<spdlog::logger> logger_;
};

#define LOG_INFO(fmt, ...) \
    Logger::instance().logWithLocation( \
        spdlog::level::info, \
        spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, \
        fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    Logger::instance().logWithLocation( \
        spdlog::level::warn, \
        spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, \
        fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    Logger::instance().logWithLocation( \
        spdlog::level::err, \
        spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, \
        fmt, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) \
    Logger::instance().logWithLocation( \
        spdlog::level::debug, \
        spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, \
        fmt, ##__VA_ARGS__)

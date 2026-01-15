#include "logger.h"
#include <filesystem>
#include <spdlog/sinks/rotating_file_sink.h>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    std::filesystem::create_directories("logs");

    std::size_t max_size = 5 * 1024 * 1024;
    std::size_t max_files = 3;

    logger_ = spdlog::rotating_logger_mt(
        "chatroom_logger",
        "logs/chatroom.log",
        max_size,
        max_files
    );
    logger_->set_level(spdlog::level::info);
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] [tid %t] [%s:%# %!] %v");
    logger_->flush_on(spdlog::level::info);
}

void Logger::setLevel(spdlog::level::level_enum level) {
    logger_->set_level(level);
}

void Logger::setPattern(const std::string& pattern) {
    logger_->set_pattern(pattern);
}

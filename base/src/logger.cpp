#include "logger.h"

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    logger_ = spdlog::default_logger();
    logger_->set_level(spdlog::level::info);
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] %v");
}

void Logger::setLevel(spdlog::level::level_enum level) {
    logger_->set_level(level);
}

void Logger::setPattern(const std::string& pattern) {
    logger_->set_pattern(pattern);
}

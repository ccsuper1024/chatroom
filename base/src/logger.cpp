#include "logger.h"
#include <filesystem>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vector>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    // Default initialization (console only for safety)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    logger_ = std::make_shared<spdlog::logger>("chatroom_logger", console_sink);
    logger_->set_level(spdlog::level::info);
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S] [%^%l%$] [tid %t] [%s:%# %!] %v");
    logger_->flush_on(spdlog::level::info);
}

void Logger::configure(bool console, const std::string& file_path, const std::string& level_str) {
    std::vector<spdlog::sink_ptr> sinks;
    
    if (console) {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }
    
    if (!file_path.empty()) {
        std::filesystem::path p(file_path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        
        std::size_t max_size = 5 * 1024 * 1024;
        std::size_t max_files = 3;
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            file_path, max_size, max_files));
    }
    
    // We reuse the existing logger instance but replace its sinks
    logger_->sinks() = sinks;
    
    spdlog::level::level_enum level = spdlog::level::info;
    if (level_str == "debug") level = spdlog::level::debug;
    else if (level_str == "warn") level = spdlog::level::warn;
    else if (level_str == "error") level = spdlog::level::err;
    else if (level_str == "trace") level = spdlog::level::trace;
    
    logger_->set_level(level);
    logger_->flush_on(level);
}

void Logger::setLevel(spdlog::level::level_enum level) {
    logger_->set_level(level);
}

void Logger::setPattern(const std::string& pattern) {
    logger_->set_pattern(pattern);
}

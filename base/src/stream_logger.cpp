#include "stream_logger.h"
#include "logger.h"

#include <iostream>
#include <ostream>
#include <string>

class StreamLoggerBuf : public std::streambuf {
public:
    StreamLoggerBuf(std::streambuf* dest, bool is_error)
        : dest_(dest), is_error_(is_error) {}

protected:
    int overflow(int ch) override {
        if (ch == traits_type::eof()) {
            return dest_->sputc(ch);
        }
        char c = static_cast<char>(ch);
        buffer_.push_back(c);
        if (c == '\n') {
            flush_line();
        }
        return dest_->sputc(c);
    }

    int sync() override {
        flush_line();
        return dest_->pubsync();
    }

private:
    void flush_line() {
        if (buffer_.empty()) {
            return;
        }
        std::string line = buffer_;
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        if (!line.empty()) {
            if (is_error_) {
                Logger::instance().error("{}", line);
            } else {
                Logger::instance().info("{}", line);
            }
        }
        buffer_.clear();
    }

    std::streambuf* dest_;
    bool is_error_;
    std::string buffer_;
};

void initLoggerForStdStreams() {
    static StreamLoggerBuf cout_buf(std::cout.rdbuf(), false);
    static StreamLoggerBuf cerr_buf(std::cerr.rdbuf(), true);
    std::cout.rdbuf(&cout_buf);
    std::cerr.rdbuf(&cerr_buf);
}

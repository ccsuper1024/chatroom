#include "client_config.h"

#include <fstream>
#include <string>
#include <algorithm>

static std::string trim(const std::string& s) {
    auto begin = s.begin();
    while (begin != s.end() && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    auto end = s.end();
    do {
        --end;
    } while (end != begin && std::isspace(static_cast<unsigned char>(*end)));
    return std::string(begin, end + 1);
}

static HeartbeatConfig loadHeartbeatConfig() {
    HeartbeatConfig cfg;
    cfg.interval_seconds = 1;
    cfg.max_retries = 1;
    cfg.client_version = "1.0.0";
    
    std::ifstream in("conf/client.yaml");
    if (!in.is_open()) {
        return cfg;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') {
            continue;
        }
        auto pos = t.find(':');
        if (pos == std::string::npos) {
            continue;
        }
        std::string key = trim(t.substr(0, pos));
        std::string value = trim(t.substr(pos + 1));
        try {
            if (key == "interval_seconds") {
                cfg.interval_seconds = std::stoi(value);
            } else if (key == "max_retries") {
                cfg.max_retries = std::stoi(value);
            } else if (key == "client_version") {
                cfg.client_version = value;
            }
        } catch (...) {
        }
    }
    if (cfg.interval_seconds <= 0) {
        cfg.interval_seconds = 1;
    }
    if (cfg.max_retries < 0) {
        cfg.max_retries = 0;
    }
    if (cfg.client_version.empty()) {
        cfg.client_version = "1.0.0";
    }
    return cfg;
}

HeartbeatConfig getHeartbeatConfig() {
    //声明了一个静态对象，用于存储配置信息。静态存储期内，对象只会被初始化一次，后续调用该函数时，直接返回该对象的引用。
    static HeartbeatConfig cfg = loadHeartbeatConfig();
    return cfg;
}

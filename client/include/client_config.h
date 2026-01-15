#pragma once

#include <string>

struct HeartbeatConfig {
    int interval_seconds;
    int max_retries;
    std::string client_version;
};

HeartbeatConfig getHeartbeatConfig();

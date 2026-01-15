#pragma once

struct HeartbeatConfig {
    int interval_seconds;
    int max_retries;
};

HeartbeatConfig getHeartbeatConfig();


#include "chatroom/session_manager.h"
#include "utils/server_config.h"
#include "logger.h"
#include <sstream>
#include <iomanip>

static std::atomic<unsigned long long> g_connection_counter{0};

SessionManager::SessionManager(std::shared_ptr<MetricsCollector> metrics)
    : metrics_collector_(metrics), running_(false) {
}

SessionManager::~SessionManager() {
    stop();
}

void SessionManager::start() {
    running_.store(true);
    cleanup_thread_ = std::thread([this]() { cleanupLoop(); });
}

void SessionManager::stop() {
    running_.store(false);
    cleanup_cv_.notify_all();
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
}

std::string SessionManager::generateConnectionId() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    unsigned long long id = ++g_connection_counter;
    std::ostringstream oss;
    oss << "conn-" << millis << "-" << id;
    return oss.str();
}

SessionManager::LoginResult SessionManager::login(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if username is already taken
    for (const auto& kv : sessions_) {
        if (kv.second.username == username) {
            return {false, "Username already taken", ""};
        }
    }

    std::string connection_id = generateConnectionId();
    UserSession session;
    session.username = username;
    session.connection_id = connection_id;
    session.last_heartbeat = std::chrono::system_clock::now();
    session.login_time = session.last_heartbeat;
    
    sessions_[connection_id] = session;
    metrics_collector_->updateActiveSessions(sessions_.size());
    
    return {true, "", connection_id};
}

bool SessionManager::updateHeartbeat(const std::string& connection_id, const std::string& client_version) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(connection_id);
    if (it != sessions_.end()) {
        it->second.client_version = client_version;
        it->second.last_heartbeat = std::chrono::system_clock::now();
        return true;
    }
    return false;
}

std::string SessionManager::getUsername(const std::string& connection_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(connection_id);
    if (it != sessions_.end()) {
        return it->second.username;
    }
    return "";
}

std::vector<UserSession> SessionManager::getAllSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UserSession> result;
    result.reserve(sessions_.size());
    for (const auto& kv : sessions_) {
        result.push_back(kv.second);
    }
    return result;
}

void SessionManager::cleanupLoop() {
    using namespace std::chrono;
    int interval = ServerConfig::instance().session_cleanup_interval_seconds;
    int timeout = ServerConfig::instance().heartbeat_timeout_seconds;
    
    while (running_.load()) {
        {
            std::unique_lock<std::mutex> lock(cleanup_mutex_);
            cleanup_cv_.wait_for(lock, std::chrono::seconds(interval), 
                [this]{ return !running_.load(); });
        }
        if (!running_.load()) break;

        auto now = system_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            auto diff = duration_cast<seconds>(now - it->second.last_heartbeat).count();
            if (diff > timeout) {
                LOG_INFO("移除超时会话: {} {}", it->second.username, it->second.connection_id);
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
        metrics_collector_->updateActiveSessions(sessions_.size());
    }
}

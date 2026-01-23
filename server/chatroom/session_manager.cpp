#include "chatroom/session_manager.h"
#include "utils/server_config.h"
#include "logger.h"
#include "net/tcp_connection.h"
#include "database_manager.h"
#include <sstream>
#include <iomanip>

static std::atomic<unsigned long long> g_connection_counter{0};

SessionManager::SessionManager(EventLoop* loop, std::shared_ptr<MetricsCollector> metrics)
    : loop_(loop), metrics_collector_(metrics) {
    timer_ = std::make_unique<TimerFd>(loop_);
    timer_->setCallback([this]() { cleanup(); });
}

SessionManager::~SessionManager() {
    stop();
}

void SessionManager::start() {
    int interval_sec = ServerConfig::instance().session_cleanup_interval_seconds;
    timer_->start(interval_sec * 1000, interval_sec * 1000);
}

void SessionManager::stop() {
    if (timer_) {
        timer_->stop();
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

SessionManager::LoginResult SessionManager::login(const std::string& username, const std::string& client_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if username is already taken
    for (const auto& kv : sessions_) {
        if (kv.second.username == username) {
            return {false, "Username already taken", "", -1};
        }
    }

    std::string connection_id = generateConnectionId();
    UserSession session;
    session.username = username;
    session.user_id = DatabaseManager::instance().getUserId(username);
    session.connection_id = connection_id;
    session.client_type = client_type;
    session.last_heartbeat = std::chrono::system_clock::now();
    session.login_time = session.last_heartbeat;
    
    sessions_[connection_id] = session;
    metrics_collector_->updateActiveSessions(sessions_.size());
    
    return {true, "", connection_id, session.user_id};
}

void SessionManager::registerSipSession(const std::string& username, std::shared_ptr<TcpConnection> conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    sip_sessions_[username] = conn;
    LOG_INFO("Registered SIP session for user: {}", username);
}

std::shared_ptr<TcpConnection> SessionManager::getSipConnection(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sip_sessions_.find(username);
    if (it != sip_sessions_.end()) {
        return it->second.lock();
    }
    return nullptr;
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

void SessionManager::cleanup() {
    using namespace std::chrono;
    int timeout = ServerConfig::instance().heartbeat_timeout_seconds;
    
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

    // Cleanup expired SIP sessions
    for (auto it = sip_sessions_.begin(); it != sip_sessions_.end();) {
        if (it->second.expired()) {
            it = sip_sessions_.erase(it);
        } else {
            ++it;
        }
    }

    metrics_collector_->updateActiveSessions(sessions_.size());
}

#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <vector>
#include <memory>
#include "utils/metrics_collector.h"

/**
 * @brief 用户会话结构体
 * 
 * 存储用户的会话信息，包括用户名、连接ID、客户端版本以及心跳和登录时间。
 */
struct UserSession {
    std::string username;           ///< 用户名
    std::string connection_id;      ///< 唯一连接标识
    std::string client_version;     ///< 客户端版本号
    std::chrono::system_clock::time_point last_heartbeat; ///< 最后心跳时间
    std::chrono::system_clock::time_point login_time;     ///< 登录时间
};

/**
 * @brief 会话管理器
 * 
 * 负责管理用户会话的创建、销毁、心跳维护以及过期清理。
 * 包含一个后台线程定期清理超时会话。
 */
class SessionManager {
public:
    /**
     * @brief 构造函数
     * @param metrics 指标收集器指针
     */
    SessionManager(std::shared_ptr<MetricsCollector> metrics);
    
    /**
     * @brief 析构函数
     * 
     * 停止清理线程并释放资源。
     */
    ~SessionManager();

    /**
     * @brief 启动会话管理器
     * 
     * 启动后台清理线程。
     */
    void start();
    
    /**
     * @brief 停止会话管理器
     * 
     * 停止清理线程。
     */
    void stop();

    // Session Management
    /**
     * @brief 登录结果结构体
     */
    struct LoginResult {
        bool success;           ///< 登录是否成功
        std::string error_msg;  ///< 错误信息（失败时）
        std::string connection_id; ///< 连接ID（成功时）
    };

    /**
     * @brief 用户登录
     * @param username 用户名
     * @return LoginResult 登录结果
     */
    LoginResult login(const std::string& username);
    
    /**
     * @brief 更新用户心跳
     * @param connection_id 连接ID
     * @param client_version 客户端版本
     * @return true 更新成功
     * @return false 会话不存在或更新失败
     */
    bool updateHeartbeat(const std::string& connection_id, const std::string& client_version);
    
    // Getters
    /**
     * @brief 根据连接ID获取用户名
     * @param connection_id 连接ID
     * @return std::string 用户名（若不存在则为空或抛出异常，具体取决于实现）
     */
    std::string getUsername(const std::string& connection_id);
    
    /**
     * @brief 获取所有活跃会话
     * @return std::vector<UserSession> 会话列表
     */
    std::vector<UserSession> getAllSessions();
    
private:
    /**
     * @brief 会话清理循环
     * 
     * 后台线程运行的函数，定期检查并移除超时会话。
     */
    void cleanupLoop();
    
    /**
     * @brief 生成唯一连接ID
     * @return std::string UUID字符串
     */
    std::string generateConnectionId();

    std::shared_ptr<MetricsCollector> metrics_collector_; ///< 指标收集器
    std::unordered_map<std::string, UserSession> sessions_; ///< 会话映射表 (connection_id -> Session)
    std::mutex mutex_;                                      ///< 会话操作互斥锁
    
    std::atomic<bool> running_;                             ///< 运行标志
    std::thread cleanup_thread_;                            ///< 清理线程
    std::mutex cleanup_mutex_;                              ///< 清理线程互斥锁
    std::condition_variable cleanup_cv_;                    ///< 清理线程条件变量
};

#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

/**
 * @brief 网路地址封装类
 * 
 * 封装 struct sockaddr_in，提供 IP 和端口的便捷转换。
 * 支持 IPv4，为后续 IPv6 扩展预留空间。
 */
class InetAddress {
public:
    /**
     * @brief 构造函数：仅指定端口，IP 默认为 INADDR_ANY
     * @param port 端口号
     * @param loopbackOnly true: 仅监听 127.0.0.1; false: 监听 0.0.0.0
     */
    explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false);

    /**
     * @brief 构造函数：指定 IP 和端口
     * @param ip IP 地址字符串
     * @param port 端口号
     */
    InetAddress(std::string ip, uint16_t port);

    /**
     * @brief 构造函数：从 struct sockaddr_in 构造
     * @param addr sockaddr_in 结构体
     */
    explicit InetAddress(const struct sockaddr_in& addr)
        : addr_(addr) {
    }

    /**
     * @brief 获取协议族 (AF_INET)
     */
    sa_family_t family() const { return addr_.sin_family; }

    /**
     * @brief 获取 IP 地址字符串
     */
    std::string toIp() const;

    /**
     * @brief 获取 IP:Port 字符串
     */
    std::string toIpPort() const;

    /**
     * @brief 获取端口号 (主机字节序)
     */
    uint16_t toPort() const;

    /**
     * @brief 获取底层 sockaddr_in 结构体 (const)
     */
    const struct sockaddr_in* getSockAddr() const { 
        return &addr_; 
    }

    /**
     * @brief 设置底层 sockaddr_in 结构体
     */
    void setSockAddr(const struct sockaddr_in& addr) { 
        addr_ = addr; 
    }

private:
    struct sockaddr_in addr_;
};

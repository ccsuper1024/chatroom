#include "net/inet_address.h"
#include <gtest/gtest.h>

TEST(InetAddressTest, ConstructorPortOnly) {
    InetAddress addr(8080);
    EXPECT_EQ(addr.toPort(), 8080);
    // Default is INADDR_ANY (0.0.0.0)
    EXPECT_EQ(addr.toIp(), "0.0.0.0");
}

TEST(InetAddressTest, ConstructorLoopback) {
    InetAddress addr(1234, true);
    EXPECT_EQ(addr.toPort(), 1234);
    EXPECT_EQ(addr.toIp(), "127.0.0.1");
}

TEST(InetAddressTest, ConstructorIpPort) {
    InetAddress addr("192.168.1.100", 9000);
    EXPECT_EQ(addr.toPort(), 9000);
    EXPECT_EQ(addr.toIp(), "192.168.1.100");
    EXPECT_EQ(addr.toIpPort(), "192.168.1.100:9000");
}

TEST(InetAddressTest, FromSockAddr) {
    struct sockaddr_in sa;
    bzero(&sa, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(5555);
    inet_pton(AF_INET, "10.0.0.5", &sa.sin_addr);

    InetAddress addr(sa);
    EXPECT_EQ(addr.toPort(), 5555);
    EXPECT_EQ(addr.toIp(), "10.0.0.5");
}

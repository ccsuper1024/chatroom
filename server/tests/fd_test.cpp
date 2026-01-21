#include <gtest/gtest.h>
#include "net/event_loop.h"
#include "net/event_loop_thread.h"
#include "net/timer_fd.h"
#include "net/event_fd.h"
#include "net/signal_fd.h"
#include "net/udp_socket.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <arpa/inet.h>

TEST(FdAbstractionTest, TimerFdTest) {
    EventLoop loop;
    TimerFd timer(&loop);
    
    std::atomic<int> count{0};
    timer.setCallback([&]() {
        count++;
        if (count >= 3) {
            timer.stop();
            loop.stop();
        }
    });
    
    // Start timer with 10ms initial delay and 10ms interval
    timer.start(10, 10);
    
    loop.loop();
    
    EXPECT_GE(count, 3);
}

TEST(FdAbstractionTest, EventFdTest) {
    EventLoopThread loopThread;
    EventLoop* loop = loopThread.startLoop();
    
    EventFd eventFd(loop);
    std::atomic<bool> signaled{false};
    
    eventFd.setCallback([&]() {
        signaled = true;
    });
    
    eventFd.notify();
    
    // Wait for event processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(signaled);
}

TEST(FdAbstractionTest, UdpSocketTest) {
    EventLoopThread loopThread;
    EventLoop* loop = loopThread.startLoop();
    
    UdpSocket receiver(loop, 9999);
    ASSERT_TRUE(receiver.bind());
    
    std::atomic<bool> received{false};
    std::string receivedData;
    
    receiver.setMessageCallback([&](const char* data, size_t len, const struct sockaddr_in& addr) {
        receivedData.assign(data, len);
        received = true;
    });
    
    // Create a sender socket (just a raw socket for simplicity or another UdpSocket)
    int senderFd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9999);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    
    std::string msg = "Hello UDP";
    sendto(senderFd, msg.data(), msg.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    close(senderFd);
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(received);
    EXPECT_EQ(receivedData, "Hello UDP");
}

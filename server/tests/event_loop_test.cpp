#include <gtest/gtest.h>
#include "net/event_loop.h"
#include "net/channel.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <chrono>

class EventLoopTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
    void TearDown() override {
    }
};

TEST_F(EventLoopTest, BasicLoop) {
    EventLoop loop;
    EXPECT_TRUE(loop.isInLoopThread());
    
    // Stop after 100ms
    std::thread t([&loop](){
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop.stop();
    });
    
    loop.loop();
    t.join();
}

TEST_F(EventLoopTest, EventFdTrigger) {
    EventLoop loop;
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ASSERT_GT(evtfd, 0);
    
    Channel channel(&loop, evtfd);
    bool triggered = false;
    
    channel.setReadCallback([&](){
        uint64_t one;
        ssize_t n = ::read(evtfd, &one, sizeof(one));
        (void)n;
        triggered = true;
        loop.stop();
    });
    channel.enableReading();
    
    // Write to eventfd after 50ms
    std::thread t([&](){
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        uint64_t one = 1;
        ssize_t n = ::write(evtfd, &one, sizeof(one));
        (void)n;
    });
    
    loop.loop();
    t.join();
    
    EXPECT_TRUE(triggered);
    channel.disableAll();
    channel.remove();
    ::close(evtfd);
}

TEST_F(EventLoopTest, QueueInLoopCrossThread) {
    EventLoop loop;
    std::atomic<bool> ran(false);
    
    std::thread t([&](){
        loop.queueInLoop([&](){
            ran = true;
            loop.stop();
        });
    });
    
    loop.loop();
    t.join();
    EXPECT_TRUE(ran);
}

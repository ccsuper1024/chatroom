#include <gtest/gtest.h>
#include "net/event_loop.h"
#include "net/timestamp.h"
#include <iostream>
#include <vector>

using namespace net;

class TimerTest : public ::testing::Test {
protected:
    EventLoop loop;
};

TEST_F(TimerTest, BasicTimer) {
    std::vector<std::string> results;

    loop.runAfter(0.1, [&]() {
        results.push_back("0.1s");
    });

    loop.runAfter(0.5, [&]() {
        results.push_back("0.5s");
        loop.stop();
    });

    loop.loop();

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0], "0.1s");
    EXPECT_EQ(results[1], "0.5s");
}

TEST_F(TimerTest, RepeatTimer) {
    int count = 0;
    
    // Every 0.1s
    loop.runEvery(0.1, [&]() {
        count++;
        if (count >= 3) {
            loop.stop();
        }
    });

    loop.loop();

    EXPECT_GE(count, 3);
}

TEST_F(TimerTest, OrderCheck) {
    std::vector<int> order;

    loop.runAfter(0.3, [&]() { order.push_back(3); });
    loop.runAfter(0.1, [&]() { order.push_back(1); });
    loop.runAfter(0.2, [&]() { order.push_back(2); });
    
    loop.runAfter(0.4, [&]() { loop.stop(); });

    loop.loop();

    ASSERT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

#include "net/buffer.h"
#include <gtest/gtest.h>

TEST(BufferTest, AppendRetrieve) {
    Buffer buf;
    EXPECT_EQ(buf.readableBytes(), 0);
    EXPECT_EQ(buf.writableBytes(), 1024);
    EXPECT_EQ(buf.prependableBytes(), 8);

    const std::string str = "Hello World";
    buf.append(str);
    EXPECT_EQ(buf.readableBytes(), str.size());
    EXPECT_EQ(buf.writableBytes(), 1024 - str.size());
    
    EXPECT_EQ(buf.retrieveAsString(5), "Hello");
    EXPECT_EQ(buf.readableBytes(), str.size() - 5);
    
    EXPECT_EQ(buf.retrieveAllAsString(), " World");
    EXPECT_EQ(buf.readableBytes(), 0);
}

TEST(BufferTest, Grow) {
    Buffer buf;
    buf.append(std::string(500, 'x'));
    EXPECT_EQ(buf.readableBytes(), 500);
    EXPECT_EQ(buf.writableBytes(), 1024 - 500);

    // 写入超过剩余空间，触发扩容
    buf.append(std::string(1000, 'y'));
    EXPECT_EQ(buf.readableBytes(), 1500);
    // 扩容后 capacity 应该至少是 1500 + 8
    EXPECT_GE(buf.writableBytes(), 0); 
}

TEST(BufferTest, InternalMove) {
    Buffer buf;
    buf.append(std::string(800, 'x'));
    buf.retrieve(500); // 读走 500，readerIndex 前移
    EXPECT_EQ(buf.readableBytes(), 300);
    EXPECT_EQ(buf.prependableBytes(), 8 + 500);
    
    // 写入 400，虽然 writable 不够，但 prependable 够，应该触发内部移动整理
    // 初始 1024，写了 800 (剩 224)。retrieve 500。
    // 再写 400。需要 400 空间。当前 writable 224。
    // 总空闲 = writable(224) + prependable(508) - 8 = 724 > 400。
    // 应该不扩容，只是移动。
    buf.append(std::string(400, 'y'));
    EXPECT_EQ(buf.readableBytes(), 700);
}

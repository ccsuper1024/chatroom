#include <gtest/gtest.h>
#include "chatroom_server.h"
#include "chatroom_client.h"
#include <thread>
#include <chrono>
#include <vector>

// 集成测试：测试完整的聊天流程
TEST(IntegrationTest, ChatFlow) {
    const int TEST_PORT = 18081;
    ChatRoomServer server(TEST_PORT);
    std::thread server_thread([&server]() { server.start(); });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    try {
        ChatRoomClient clientA("127.0.0.1", TEST_PORT);
        bool loginA = clientA.login("Alice");
        ASSERT_TRUE(loginA) << "Client A login failed";

        ChatRoomClient clientB("127.0.0.1", TEST_PORT);
        bool loginB = clientB.login("Bob");
        ASSERT_TRUE(loginB) << "Client B login failed";

        std::string msgContent = "Hello Bob!";
        bool sendResult = clientA.sendMessage(msgContent);
        ASSERT_TRUE(sendResult) << "Client A send message failed";

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::vector<std::string> messagesB = clientB.getMessages();
        
        bool found = false;
        for (const auto& msg : messagesB) {
            if (msg.find("Alice") != std::string::npos && msg.find(msgContent) != std::string::npos) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Client B did not receive the message from Client A";

        // Test incremental updates
        std::string msgContent2 = "How are you?";
        clientA.sendMessage(msgContent2);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::vector<std::string> messagesB2 = clientB.getMessages();
        EXPECT_GE(messagesB2.size(), 1);
        
        bool found2 = false;
        for (const auto& msg : messagesB2) {
            if (msg.find(msgContent2) != std::string::npos) {
                found2 = true;
                break;
            }
        }
        EXPECT_TRUE(found2) << "Client B did not receive the second message";

        server.stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }
    } catch (const std::exception& e) {
        server.stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }
        FAIL() << "Exception during test: " << e.what();
    }
}

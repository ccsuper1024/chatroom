#include <gtest/gtest.h>
#include "chatroom/chatroom_server.h"
#include "utils/server_config.h"
#include <thread>
#include <chrono>

// Test Fixture allowing access to private members
class ChatRoomServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset config for each test if needed
        // ServerConfig::instance()...
    }

    void TearDown() override {
    }

    // Helper wrappers to access private methods
    bool validateUsername(ChatRoomServer& server, const std::string& username) {
        return server.validateUsername(username);
    }

    bool validateMessage(ChatRoomServer& server, const std::string& content) {
        return server.validateMessage(content);
    }

    bool checkRateLimit(ChatRoomServer& server, const std::string& ip) {
        return server.checkRateLimit(ip);
    }
};

TEST_F(ChatRoomServerTest, ValidateUsername) {
    ChatRoomServer server(9999);
    
    // Valid usernames
    EXPECT_TRUE(validateUsername(server, "alice"));
    EXPECT_TRUE(validateUsername(server, "bob123"));
    EXPECT_TRUE(validateUsername(server, "user_name"));
    
    // Invalid usernames
    EXPECT_FALSE(validateUsername(server, "")); // Empty
    EXPECT_FALSE(validateUsername(server, "invalid@char")); // Special char
    EXPECT_FALSE(validateUsername(server, "space name")); // Space
    
    // Length limit (assuming default is 32)
    std::string long_name(33, 'a');
    EXPECT_FALSE(validateUsername(server, long_name));
}

TEST_F(ChatRoomServerTest, ValidateMessage) {
    ChatRoomServer server(9999);
    
    // Valid messages
    EXPECT_TRUE(validateMessage(server, "Hello world"));
    EXPECT_TRUE(validateMessage(server, "Hello\nWorld")); // Newline allowed
    EXPECT_TRUE(validateMessage(server, "Hello\tWorld")); // Tab allowed
    
    // Invalid messages
    EXPECT_FALSE(validateMessage(server, "")); // Empty
    
    // Control chars (e.g. 0x01)
    std::string ctrl_msg = "Hello";
    ctrl_msg += (char)0x01;
    EXPECT_FALSE(validateMessage(server, ctrl_msg));
    
    // Length limit (assuming default is 4096)
    std::string long_msg(4097, 'a');
    EXPECT_FALSE(validateMessage(server, long_msg));
}

TEST_F(ChatRoomServerTest, RateLimiting) {
    ChatRoomServer server(9999);
    
    // Config: enable rate limit
    auto& config = ServerConfig::instance();
    bool original_enabled = config.rate_limit.enabled;
    int original_max = config.rate_limit.max_requests;
    int original_window = config.rate_limit.window_seconds;
    
    config.rate_limit.enabled = true;
    config.rate_limit.max_requests = 5;
    config.rate_limit.window_seconds = 1;
    
    std::string ip = "127.0.0.1";
    
    // First 5 requests should pass
    for(int i=0; i<5; ++i) {
        EXPECT_TRUE(checkRateLimit(server, ip)) << "Request " << i << " failed";
    }
    
    // 6th request should fail
    EXPECT_FALSE(checkRateLimit(server, ip)) << "Request 6 should be limited";
    
    // Restore config
    config.rate_limit.enabled = original_enabled;
    config.rate_limit.max_requests = original_max;
    config.rate_limit.window_seconds = original_window;
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

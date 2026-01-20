#include <gtest/gtest.h>
#include "chatroom_server.h"
#include "chatroom_client.h"
#include "server_config.h"
#include <thread>
#include <chrono>
#include <vector>
#include <filesystem>
#include <atomic>
#include <sys/socket.h>
#include <arpa/inet.h>

// Helper to send raw data
std::string SendRaw(int port, const std::string& data) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";
    
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return "";
    }
    
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent = send(sock, data.c_str() + total_sent, data.size() - total_sent, 0);
        if (sent < 0) break;
        total_sent += sent;
    }
    
    // Shutdown write to signal end of request if needed, but for HTTP we usually rely on Content-Length
    // shutdown(sock, SHUT_WR);
    
    char buffer[4096];
    std::string response;
    // Simple read with timeout
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    while (true) {
        ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
        if (n <= 0) break;
        response.append(buffer, n);
    }
    close(sock);
    return response;
}

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
        auto messagesB = clientB.getMessages();
        
        bool found = false;
        for (const auto& msg : messagesB) {
            if (msg.username == "Alice" && msg.content == msgContent) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Client B did not receive the message from Client A";

        // Test incremental updates
        std::string msgContent2 = "How are you?";
        clientA.sendMessage(msgContent2);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        auto messagesB2 = clientB.getMessages();
        EXPECT_GE(messagesB2.size(), 1);
        
        bool found2 = false;
        for (const auto& msg : messagesB2) {
            if (msg.content == msgContent2) {
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

// 持久化测试：重启服务器后消息应保留
TEST(IntegrationTest, PersistenceTest) {
    const int TEST_PORT = 18082;
    std::string test_history_file = "test_data/history_test.json";
    
    // Set config
    ServerConfig::instance().history_file_path = test_history_file;
    
    // Ensure clean state
    if (std::filesystem::exists(test_history_file)) {
        std::filesystem::remove(test_history_file);
    }

    // 1. Start server and send a message
    {
        ChatRoomServer server(TEST_PORT);
        std::thread server_thread([&server]() { server.start(); });
        std::this_thread::sleep_for(std::chrono::seconds(1));

        try {
            ChatRoomClient client("127.0.0.1", TEST_PORT);
            if (client.login("PersistUser")) {
                client.sendMessage("Persistent Message");
            }
        } catch (...) {}

        server.stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 2. Restart server and verify message exists
    {
        ChatRoomServer server(TEST_PORT);
        std::thread server_thread([&server]() { server.start(); });
        std::this_thread::sleep_for(std::chrono::seconds(1));

        try {
            ChatRoomClient client("127.0.0.1", TEST_PORT);
            ASSERT_TRUE(client.login("Checker"));
            
            auto messages = client.getMessages();
            bool found = false;
            for (const auto& msg : messages) {
                if (msg.username == "PersistUser" && msg.content == "Persistent Message") {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found) << "Message should persist after restart";

        } catch (const std::exception& e) {
            server.stop();
            if (server_thread.joinable()) server_thread.join();
            FAIL() << "Exception: " << e.what();
        }

        server.stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }
    
    // Cleanup
    if (std::filesystem::exists(test_history_file)) {
        std::filesystem::remove(test_history_file);
    }
}

// 并发测试：多用户同时登录发送消息
TEST(IntegrationTest, ConcurrencyTest) {
    const int TEST_PORT = 18083;
    
    // Temporarily increase rate limit for test
    auto& config = ServerConfig::instance();
    auto original_limit = config.rate_limit.max_requests;
    config.rate_limit.max_requests = 2000;

    ChatRoomServer server(TEST_PORT);
    std::thread server_thread([&server]() { server.start(); });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    const int CLIENT_COUNT = 20;
    const int MSGS_PER_CLIENT = 5;
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < CLIENT_COUNT; ++i) {
        threads.emplace_back([i, TEST_PORT, &success_count]() {
            try {
                std::string username = "User" + std::to_string(i);
                ChatRoomClient client("127.0.0.1", TEST_PORT);
                // Retry login if failed due to contention/limit (though limit is raised)
                bool logged_in = false;
                for(int r=0; r<3; ++r) {
                    if(client.login(username)) {
                        logged_in = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                if (!logged_in) return;

                for (int j = 0; j < MSGS_PER_CLIENT; ++j) {
                    if (!client.sendMessage("Msg " + std::to_string(j))) return;
                    // Small delay to allow some interleaving
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                success_count++;
            } catch (...) {
                // Ignore errors in threads, success_count will tell
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    EXPECT_EQ(success_count, CLIENT_COUNT) << "Not all clients completed successfully";

    // Verify server is still responsive
    try {
        ChatRoomClient admin("127.0.0.1", TEST_PORT);
        if (admin.login("Admin")) {
            auto stats_str = admin.getStats();
            EXPECT_FALSE(stats_str.empty());
        } else {
            ADD_FAILURE() << "Admin login failed";
        }
    } catch (...) {
        ADD_FAILURE() << "Server failed to respond after concurrency test";
    }

    server.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    // Restore config
    config.rate_limit.max_requests = original_limit;
}

// 安全与异常测试：畸形包、超大包等
TEST(IntegrationTest, SecurityTest) {
    const int TEST_PORT = 18085;
    ChatRoomServer server(TEST_PORT);
    std::thread server_thread([&server]() { server.start(); });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 1. Malformed HTTP Request (Garbage)
    {
        std::string resp = SendRaw(TEST_PORT, "NOT_HTTP_REQUEST\r\n\r\n");
        EXPECT_NE(resp.find("400 Error"), std::string::npos) << "Should return 400 status for garbage";
        EXPECT_NE(resp.find("\"error_code\":1001"), std::string::npos) << "Should contain INVALID_REQUEST code";
    }

    // 2. Malformed JSON Body
    {
        std::string req = "POST /login HTTP/1.1\r\nContent-Length: 5\r\n\r\nHELLO";
        std::string resp = SendRaw(TEST_PORT, req);
        EXPECT_NE(resp.find("400 Error"), std::string::npos) << "Should return 400 status for bad json";
        EXPECT_NE(resp.find("\"error_code\":1001"), std::string::npos) << "Should contain INVALID_REQUEST code";
    }
    
    // 3. Payload Too Large (> 10MB)
    {
        // Allocate ~10.1MB
        std::string huge(10 * 1024 * 1024 + 100 * 1024, 'X'); 
        std::string resp = SendRaw(TEST_PORT, huge);
        
        // Expect 413
        EXPECT_NE(resp.find("413 Error"), std::string::npos) << "Should return 413 status for huge payload";
        EXPECT_NE(resp.find("\"error_code\":1006"), std::string::npos) << "Should contain PAYLOAD_TOO_LARGE code";
    }

    server.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

// 监控指标测试：Prometheus 格式验证
TEST(IntegrationTest, PrometheusMetrics) {
    const int TEST_PORT = 18086;
    ChatRoomServer server(TEST_PORT);
    std::thread server_thread([&server]() { server.start(); });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Make some requests to generate metrics
    {
        ChatRoomClient client("127.0.0.1", TEST_PORT);
        client.login("MetricsUser");
        client.sendMessage("Testing Metrics");
    }

    // Request metrics
    std::string req = "GET /metrics HTTP/1.1\r\nHost: localhost\r\n\r\n";
    std::string resp = SendRaw(TEST_PORT, req);

    // Verify response headers
    EXPECT_NE(resp.find("Content-Type: text/plain; version=0.0.4"), std::string::npos) 
        << "Should return Prometheus content type";

    // Verify Prometheus format
    EXPECT_NE(resp.find("# HELP chatroom_uptime_seconds"), std::string::npos);
    EXPECT_NE(resp.find("# TYPE chatroom_requests_total counter"), std::string::npos);
    EXPECT_NE(resp.find("chatroom_requests_total{method=\"POST\",path=\"/login\"}"), std::string::npos);
    EXPECT_NE(resp.find("chatroom_stored_messages"), std::string::npos);

    server.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

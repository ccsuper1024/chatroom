#include <gtest/gtest.h>
#include "chatroom_server.h"
#include "metrics_collector.h"
#include "server_config.h"
#include "database_manager.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ChatRoomServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup config for test
        auto& config = ServerConfig::instance();
        config.db.type = "sqlite";
        config.db.path = ":memory:"; // Use in-memory DB for tests
        config.rate_limit.enabled = true; // Ensure rate limit is enabled for testing
        config.rate_limit.max_requests = 60;
        config.rate_limit.window_seconds = 60;

        // Init DB
        DatabaseManager::instance().init(config.db);

        server_ = std::make_unique<ChatRoomServer>(8080);
    }

    void TearDown() override {
        server_->stop();
    }

    HttpResponse CallHandleLogin(const std::string& body, const std::string& ip = "127.0.0.1") {
        HttpRequest req;
        req.method = "POST";
        req.path = "/login";
        req.body = body;
        req.remote_ip = ip;
        return server_->handleLogin(req);
    }

    HttpResponse CallHandleSendMessage(const std::string& body, const std::string& ip = "127.0.0.1") {
        HttpRequest req;
        req.method = "POST";
        req.path = "/send";
        req.body = body;
        req.remote_ip = ip;
        return server_->handleSendMessage(req);
    }
    
    HttpResponse CallHandleGetMessages(const std::string& path, const std::string& ip = "127.0.0.1") {
        HttpRequest req;
        req.method = "GET";
        req.path = path;
        req.remote_ip = ip;
        return server_->handleGetMessages(req);
    }

    json GetMetrics() {
        return server_->metrics_collector_->getMetrics();
    }

    std::unique_ptr<ChatRoomServer> server_;
};

TEST_F(ChatRoomServerTest, LoginSuccess) {
    json req_body;
    req_body["username"] = "testuser";
    
    HttpResponse resp = CallHandleLogin(req_body.dump());
    
    EXPECT_EQ(resp.status_code, 200);
    
    auto body = json::parse(resp.body);
    EXPECT_TRUE(body["success"]);
    EXPECT_EQ(body["username"], "testuser");
    EXPECT_FALSE(body["connection_id"].get<std::string>().empty());
}

TEST_F(ChatRoomServerTest, LoginInvalidUsername) {
    json req_body;
    req_body["username"] = "invalid name!"; // Contains space and !
    
    HttpResponse resp = CallHandleLogin(req_body.dump());
    
    EXPECT_EQ(resp.status_code, 400);
    
    auto body = json::parse(resp.body);
    EXPECT_FALSE(body["success"]);
    EXPECT_EQ(body["error_code"], 1002); // INVALID_USERNAME
}

TEST_F(ChatRoomServerTest, SendMessageSuccess) {
    // First login
    json login_body;
    login_body["username"] = "sender";
    auto login_resp = CallHandleLogin(login_body.dump());
    auto login_resp_json = json::parse(login_resp.body);
    std::string conn_id = login_resp_json["connection_id"];
    
    // Then send message
    json send_body;
    send_body["connection_id"] = conn_id;
    send_body["content"] = "Hello World";
    
    HttpResponse resp = CallHandleSendMessage(send_body.dump());
    
    EXPECT_EQ(resp.status_code, 200);
    auto body = json::parse(resp.body);
    EXPECT_TRUE(body["success"]);
}

TEST_F(ChatRoomServerTest, SendMessageRateLimit) {
    json req_body;
    req_body["username"] = "spammer";
    req_body["content"] = "spam";
    
    // Exhaust rate limit (assuming 60 per minute)
    for (int i = 0; i < 60; ++i) {
        CallHandleSendMessage(req_body.dump(), "1.2.3.4");
    }
    
    // Next one should fail
    HttpResponse resp = CallHandleSendMessage(req_body.dump(), "1.2.3.4");
    EXPECT_EQ(resp.status_code, 429);
    
    auto body = json::parse(resp.body);
    EXPECT_EQ(body["error_code"], 1004); // RATE_LIMITED
}

TEST_F(ChatRoomServerTest, MetricsCollectorIntegration) {
    // Login
    json login_body;
    login_body["username"] = "metrics_user";
    CallHandleLogin(login_body.dump());
    
    // Send message
    json send_body;
    send_body["username"] = "metrics_user";
    send_body["content"] = "metrics test";
    CallHandleSendMessage(send_body.dump());
    
    // Check metrics
    auto metrics = GetMetrics();
    
    EXPECT_EQ(metrics["requests"]["POST /login"], 1);
    EXPECT_EQ(metrics["requests"]["POST /send"], 1);
    EXPECT_EQ(metrics["message_count"], 1);
}

TEST_F(ChatRoomServerTest, PrivateMessageTest) {
    // 1. User A sends private message to User B
    json send_body;
    send_body["username"] = "UserA";
    send_body["content"] = "Secret for B";
    send_body["target_user"] = "UserB";
    
    HttpResponse resp = CallHandleSendMessage(send_body.dump());
    EXPECT_EQ(resp.status_code, 200);

    // 2. User B fetches messages
    HttpResponse respB = CallHandleGetMessages("/messages?since=0&username=UserB");
    auto bodyB = json::parse(respB.body);
    EXPECT_TRUE(bodyB["success"]);
    bool found = false;
    for (const auto& msg : bodyB["messages"]) {
        if (msg["content"] == "Secret for B" && msg.contains("target_user") && msg["target_user"] == "UserB") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    // 3. User C fetches messages (should not see it)
    HttpResponse respC = CallHandleGetMessages("/messages?since=0&username=UserC");
    auto bodyC = json::parse(respC.body);
    bool foundC = false;
    for (const auto& msg : bodyC["messages"]) {
        if (msg["content"] == "Secret for B") {
            foundC = true;
            break;
        }
    }
    EXPECT_FALSE(foundC);
}

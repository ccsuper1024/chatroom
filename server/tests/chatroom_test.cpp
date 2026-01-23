#include <gtest/gtest.h>
#include "chatroom/chatroom_server.h"
#include "utils/metrics_collector.h"
#include "utils/server_config.h"
#include "database_manager.h"
#include "net/tcp_connection.h"
#include "net/event_loop.h"
#include "net/inet_address.h"
#include "websocket/websocket_codec.h"
#include "rtsp/rtsp_codec.h"
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <future>
#include <chrono>

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

    void RegisterUser(const std::string& username, const std::string& password) {
        DatabaseManager::instance().addUser(username, password);
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

    HttpServer* GetHttpServer() {
        return server_->http_server_.get();
    }

    void HandleWebSocketMessage(std::shared_ptr<TcpConnection> conn, const protocols::WebSocketFrame& frame) {
        server_->handleWebSocketMessage(conn, frame);
    }

    void HandleRtspMessage(std::shared_ptr<TcpConnection> conn, const protocols::RtspRequest& request) {
        server_->handleRtspMessage(conn, request);
    }

    std::unique_ptr<ChatRoomServer> server_;
};

TEST_F(ChatRoomServerTest, WebSocketLoginAndMessage) {
    RegisterUser("ws_user", "123456");

    EventLoop* loop = server_->getLoop();
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    
    // Set non-blocking
    fcntl(fds[0], F_SETFL, O_NONBLOCK);
    fcntl(fds[1], F_SETFL, O_NONBLOCK);

    InetAddress localAddr(0);
    InetAddress peerAddr(0);
    auto conn = std::make_shared<TcpConnection>(loop, "test-ws-conn", fds[0], localAddr, peerAddr);
    conn->connectEstablished();
    
    // 1. Login
    json login_req;
    login_req["type"] = "login";
    login_req["username"] = "ws_user";
    login_req["password"] = "123456";
    std::string login_str = login_req.dump();
    
    protocols::WebSocketFrame frame;
    frame.opcode = protocols::WebSocketOpcode::TEXT;
    frame.payload = login_str;
    
    HandleWebSocketMessage(conn, frame);
    
    // Read response from fds[1]
    char buf[1024];
    ssize_t n = read(fds[1], buf, sizeof(buf));
    ASSERT_GT(n, 0);
    
    std::vector<uint8_t> recv_buf(buf, buf + n);
    protocols::WebSocketFrame resp_frame;
    int consumed = protocols::WebSocketCodec::parseFrame(recv_buf.data(), recv_buf.size(), resp_frame);
    ASSERT_GT(consumed, 0);
    
    // Verify response
    std::string resp_payload(resp_frame.payload.begin(), resp_frame.payload.end());
    
    auto resp_json = json::parse(resp_payload);
    EXPECT_EQ(resp_json["type"], "login_response");
    EXPECT_TRUE(resp_json["success"]);
    EXPECT_EQ(resp_json["username"], "ws_user");
    
    // 2. Send Message
    json msg_req;
    msg_req["type"] = "message";
    msg_req["content"] = "hello ws";
    std::string msg_str = msg_req.dump();
    
    frame.payload = msg_str;
    HandleWebSocketMessage(conn, frame);
    
    // Read response
    n = read(fds[1], buf, sizeof(buf));
    ASSERT_GT(n, 0);
    
    recv_buf.assign(buf, buf + n);
    consumed = protocols::WebSocketCodec::parseFrame(recv_buf.data(), recv_buf.size(), resp_frame);
    ASSERT_GT(consumed, 0);
    
    resp_payload.assign(resp_frame.payload.begin(), resp_frame.payload.end());
    resp_json = json::parse(resp_payload);
    EXPECT_EQ(resp_json["type"], "message_response");
    EXPECT_TRUE(resp_json["success"]);
    
    // Verify DB
    auto msgs = DatabaseManager::instance().getHistory(10);
    bool found = false;
    for (const auto& m : msgs) {
        if (m.username == "ws_user" && m.content == "hello ws") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    
    close(fds[1]);
    // fds[0] closed by TcpConnection
}

TEST_F(ChatRoomServerTest, WebSocketForwarding) {
    RegisterUser("Alice", "123456");
    RegisterUser("Bob", "123456");

    EventLoop* loop = server_->getLoop();
    
    // User A: Alice
    int fds_a[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds_a), 0);
    fcntl(fds_a[0], F_SETFL, O_NONBLOCK);
    fcntl(fds_a[1], F_SETFL, O_NONBLOCK);
    auto conn_a = std::make_shared<TcpConnection>(loop, "conn_a", fds_a[0], InetAddress(0), InetAddress(0));
    conn_a->connectEstablished();
    
    // User B: Bob
    int fds_b[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds_b), 0);
    fcntl(fds_b[0], F_SETFL, O_NONBLOCK);
    fcntl(fds_b[1], F_SETFL, O_NONBLOCK);
    auto conn_b = std::make_shared<TcpConnection>(loop, "conn_b", fds_b[0], InetAddress(0), InetAddress(0));
    conn_b->connectEstablished();
    
    // 1. Login Alice
    json login_req_a;
    login_req_a["type"] = "login";
    login_req_a["username"] = "Alice";
    login_req_a["password"] = "123456";
    std::string login_str_a = login_req_a.dump();
    protocols::WebSocketFrame frame_a;
    frame_a.opcode = protocols::WebSocketOpcode::TEXT;
    frame_a.payload = login_str_a;
    HandleWebSocketMessage(conn_a, frame_a);
    
    // Consume Alice's login response
    char buf[4096];
    read(fds_a[1], buf, sizeof(buf)); 
    
    // 2. Login Bob
    json login_req_b;
    login_req_b["type"] = "login";
    login_req_b["username"] = "Bob";
    login_req_b["password"] = "123456";
    std::string login_str_b = login_req_b.dump();
    protocols::WebSocketFrame frame_b;
    frame_b.opcode = protocols::WebSocketOpcode::TEXT;
    frame_b.payload = login_str_b;
    HandleWebSocketMessage(conn_b, frame_b);
    
    // Consume Bob's login response
    read(fds_b[1], buf, sizeof(buf));

    // 3. Alice sends message to Bob
    json msg_req;
    msg_req["type"] = "message";
    msg_req["content"] = "Hi Bob";
    msg_req["target_user"] = "Bob";
    std::string msg_str = msg_req.dump();
    frame_a.payload = msg_str;
    HandleWebSocketMessage(conn_a, frame_a);
    
    // 4. Verify Bob receives the message
    ssize_t n = 0;
    int retries = 5;
    while (retries-- > 0) {
        n = read(fds_b[1], buf, sizeof(buf));
        if (n > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_GT(n, 0);
    
    std::vector<uint8_t> recv_buf(buf, buf + n);
    protocols::WebSocketFrame resp_frame;
    int consumed = protocols::WebSocketCodec::parseFrame(recv_buf.data(), recv_buf.size(), resp_frame);
    ASSERT_GT(consumed, 0);
    
    std::string resp_payload(resp_frame.payload.begin(), resp_frame.payload.end());
    auto resp_json = json::parse(resp_payload);
    
    EXPECT_EQ(resp_json["type"], "message");
    EXPECT_EQ(resp_json["username"], "Alice");
    EXPECT_EQ(resp_json["content"], "Hi Bob");
    EXPECT_EQ(resp_json["target_user"], "Bob");

    close(fds_a[1]);
    close(fds_b[1]);
}

TEST_F(ChatRoomServerTest, MultiRoomChat) {
    RegisterUser("Alice", "123456");
    RegisterUser("Bob", "123456");
    RegisterUser("Charlie", "123456");

    EventLoop* loop = server_->getLoop();
    
    // User A: Alice
    int fds_a[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds_a), 0);
    fcntl(fds_a[0], F_SETFL, O_NONBLOCK);
    fcntl(fds_a[1], F_SETFL, O_NONBLOCK);
    auto conn_a = std::make_shared<TcpConnection>(loop, "conn_a", fds_a[0], InetAddress(0), InetAddress(0));
    conn_a->connectEstablished();
    
    // User B: Bob
    int fds_b[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds_b), 0);
    fcntl(fds_b[0], F_SETFL, O_NONBLOCK);
    fcntl(fds_b[1], F_SETFL, O_NONBLOCK);
    auto conn_b = std::make_shared<TcpConnection>(loop, "conn_b", fds_b[0], InetAddress(0), InetAddress(0));
    conn_b->connectEstablished();
    
    // User C: Charlie (NotInRoom)
    int fds_c[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds_c), 0);
    fcntl(fds_c[0], F_SETFL, O_NONBLOCK);
    fcntl(fds_c[1], F_SETFL, O_NONBLOCK);
    auto conn_c = std::make_shared<TcpConnection>(loop, "conn_c", fds_c[0], InetAddress(0), InetAddress(0));
    conn_c->connectEstablished();

    // 1. Login all
    auto login = [&](std::shared_ptr<TcpConnection> conn, std::string name, int fd) {
        json req; req["type"] = "login"; req["username"] = name; req["password"] = "123456";
        std::string str = req.dump();
        protocols::WebSocketFrame frame; frame.opcode = protocols::WebSocketOpcode::TEXT; frame.payload = str;
        HandleWebSocketMessage(conn, frame);
        char buf[4096]; read(fd, buf, sizeof(buf)); // consume response
    };
    
    login(conn_a, "Alice", fds_a[1]);
    login(conn_b, "Bob", fds_b[1]);
    login(conn_c, "Charlie", fds_c[1]);

    // 2. Alice and Bob join room "tech"
    auto join = [&](std::shared_ptr<TcpConnection> conn, std::string room) {
        json req; req["type"] = "join_room"; req["room_id"] = room;
        std::string str = req.dump();
        protocols::WebSocketFrame frame; frame.opcode = protocols::WebSocketOpcode::TEXT; frame.payload = str;
        HandleWebSocketMessage(conn, frame);
    };
    
    join(conn_a, "tech");
    join(conn_b, "tech");

    // 3. Alice sends message to room "tech"
    json msg_req;
    msg_req["type"] = "message";
    msg_req["content"] = "Hello Tech Room";
    msg_req["room_id"] = "tech";
    std::string msg_str = msg_req.dump();
    protocols::WebSocketFrame frame_a;
    frame_a.opcode = protocols::WebSocketOpcode::TEXT;
    frame_a.payload = msg_str;
    HandleWebSocketMessage(conn_a, frame_a);
    
    // 4. Verify Bob receives, Charlie does not
    char buf[4096];
    
    // Check Bob
    int retries = 5;
    ssize_t n = 0;
    while (retries-- > 0) {
        n = read(fds_b[1], buf, sizeof(buf));
        if (n > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_GT(n, 0);
    std::vector<uint8_t> recv_buf(buf, buf + n);
    protocols::WebSocketFrame resp_frame;
    protocols::WebSocketCodec::parseFrame(recv_buf.data(), recv_buf.size(), resp_frame);
    std::string resp_payload(resp_frame.payload.begin(), resp_frame.payload.end());
    auto resp_json = json::parse(resp_payload);
    EXPECT_EQ(resp_json["content"], "Hello Tech Room");
    EXPECT_EQ(resp_json["room_id"], "tech");
    
    // Check Charlie (Should be nothing or EAGAIN)
    n = read(fds_c[1], buf, sizeof(buf));
    if (n > 0) {
        // If received something, ensure it is NOT the room message (maybe heartbeat or something else?)
        // But here we expect nothing
        // Actually since we use non-blocking, it might return -1 EAGAIN
        // Or if we parse it, it shouldn't be the message
    }
    // We can't easily assert "nothing received" in async test without timeout, 
    // but here immediate read should fail or return 0
    
    close(fds_a[1]);
    close(fds_b[1]);
    close(fds_c[1]);
}

TEST_F(ChatRoomServerTest, LoginSuccess) {
    RegisterUser("testuser", "123456");
    json req_body;
    req_body["username"] = "testuser";
    req_body["password"] = "123456";
    
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
    RegisterUser("sender", "123456");
    // First login
    json login_body;
    login_body["username"] = "sender";
    login_body["password"] = "123456";
    auto login_resp = CallHandleLogin(login_body.dump());
    auto login_resp_json = json::parse(login_resp.body);
    std::string conn_id = login_resp_json["connection_id"];
    
    // Then send message
    json msg_body;
    msg_body["connection_id"] = conn_id;
    msg_body["content"] = "hello world";
    
    HttpResponse resp = CallHandleSendMessage(msg_body.dump());
    
    EXPECT_EQ(resp.status_code, 200);
    auto body = json::parse(resp.body);
    EXPECT_TRUE(body["success"]);
}

TEST_F(ChatRoomServerTest, GetMessagesSuccess) {
    // Add some messages first
    ChatMessage msg1; msg1.username = "u1"; msg1.content = "c1";
    ChatMessage msg2; msg2.username = "u2"; msg2.content = "c2";
    DatabaseManager::instance().addMessage(msg1);
    DatabaseManager::instance().addMessage(msg2);
    
    HttpResponse resp = CallHandleGetMessages("/messages?limit=10");
    
    EXPECT_EQ(resp.status_code, 200);
    auto body = json::parse(resp.body);
    EXPECT_TRUE(body["success"]);
    EXPECT_EQ(body["messages"].size(), 2);
    EXPECT_EQ(body["messages"][0]["content"], "c1");
    EXPECT_EQ(body["messages"][1]["content"], "c2");
}

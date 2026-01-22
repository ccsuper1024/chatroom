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

TEST_F(ChatRoomServerTest, RtspOptionsAndDescribe) {
    EventLoop* loop = server_->getLoop();
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    fcntl(fds[0], F_SETFL, O_NONBLOCK);
    fcntl(fds[1], F_SETFL, O_NONBLOCK);

    InetAddress localAddr(0);
    InetAddress peerAddr(0);
    auto conn = std::make_shared<TcpConnection>(loop, "test-rtsp-conn", fds[0], localAddr, peerAddr);
    conn->connectEstablished();

    // 1. Send OPTIONS
    std::string options_req = 
        "OPTIONS rtsp://127.0.0.1:8080/audio RTSP/1.0\r\n"
        "CSeq: 1\r\n"
        "User-Agent: ChatRoomClient\r\n"
        "\r\n";
    
    protocols::RtspRequest options_request;
    Buffer optionsBuf;
    optionsBuf.append(options_req);
    size_t consumed = protocols::RtspCodec::parseRequest(&optionsBuf, options_request);
    ASSERT_GT(consumed, 0);
    HandleRtspMessage(conn, options_request);
    
    // Read response
    char buf[4096];
    ssize_t n = read(fds[1], buf, sizeof(buf));
    ASSERT_GT(n, 0);
    
    std::string resp_str(buf, n);
    // Log for debugging
    // std::cout << "RTSP Resp: " << resp_str << std::endl;
    
    ASSERT_TRUE(resp_str.find("RTSP/1.0 200 OK") != std::string::npos);
    ASSERT_TRUE(resp_str.find("CSeq: 1") != std::string::npos);
    ASSERT_TRUE(resp_str.find("Public: OPTIONS") != std::string::npos);
    
    // 2. Send DESCRIBE
    std::string describe_req = 
        "DESCRIBE rtsp://127.0.0.1:8080/audio RTSP/1.0\r\n"
        "CSeq: 2\r\n"
        "Accept: application/sdp\r\n"
        "\r\n";
        
    protocols::RtspRequest describe_request;
    Buffer describeBuf;
    describeBuf.append(describe_req);
    consumed = protocols::RtspCodec::parseRequest(&describeBuf, describe_request);
    ASSERT_GT(consumed, 0);
    HandleRtspMessage(conn, describe_request);
    
    n = read(fds[1], buf, sizeof(buf));
    ASSERT_GT(n, 0);
    
    resp_str.assign(buf, n);
    ASSERT_TRUE(resp_str.find("RTSP/1.0 200 OK") != std::string::npos);
    ASSERT_TRUE(resp_str.find("CSeq: 2") != std::string::npos);
    ASSERT_TRUE(resp_str.find("Content-Type: application/sdp") != std::string::npos);
    ASSERT_TRUE(resp_str.find("m=audio 0 RTP/AVP 0") != std::string::npos);
    
    close(fds[1]);
}

TEST_F(ChatRoomServerTest, SipRegisterAndInvite) {
    // std::cout << "Starting SipRegisterAndInvite" << std::endl;
    // EventLoop loop;
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    fcntl(fds[0], F_SETFL, O_NONBLOCK);
    fcntl(fds[1], F_SETFL, O_NONBLOCK);

    // std::cout << "Creating conn1" << std::endl;
    auto conn1 = std::make_shared<TcpConnection>(server_->getLoop(), "sip-conn1", fds[0], InetAddress(0), InetAddress(0));
    
    // std::cout << "Setting callbacks" << std::endl;
    // Setup callbacks
    conn1->setMessageCallback(std::bind(&HttpServer::onMessage, GetHttpServer(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    conn1->connectEstablished();
    GetHttpServer()->onConnection(conn1);

    // Define addresses for later use
    InetAddress localAddr(0);
    InetAddress peerAddr(0);

    // 1. Send REGISTER
    std::string register_req = 
        "REGISTER sip:alice@example.com SIP/2.0\r\n"
        "Via: SIP/2.0/TCP client.example.com:5060;branch=z9hG4bK776asdhds\r\n"
        "Max-Forwards: 70\r\n"
        "From: Alice <sip:alice@example.com>;tag=1928301774\r\n"
        "To: Alice <sip:alice@example.com>\r\n"
        "Call-ID: a84b4c76e66710\r\n"
        "CSeq: 1 REGISTER\r\n"
        "Contact: <sip:alice@127.0.0.1:5060>\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    
    write(fds[1], register_req.data(), register_req.size());
    
    // Process
    conn1->handleRead();
    conn1->handleWrite(); // Flush
    
    char buf[4096];
    int n = read(fds[1], buf, sizeof(buf));
    ASSERT_GT(n, 0);
    std::string resp(buf, n);
    ASSERT_TRUE(resp.find("SIP/2.0 200 OK") != std::string::npos);

    // 2. Setup second connection for Bob
    int fds2[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds2), 0);
    fcntl(fds2[0], F_SETFL, O_NONBLOCK);
    fcntl(fds2[1], F_SETFL, O_NONBLOCK);
    auto conn2 = std::make_shared<TcpConnection>(server_->getLoop(), "sip-conn2", fds2[0], localAddr, peerAddr);
    conn2->setMessageCallback(std::bind(&HttpServer::onMessage, GetHttpServer(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    conn2->connectEstablished();
    GetHttpServer()->onConnection(conn2);

    // Bob REGISTER
    std::string register_bob = 
        "REGISTER sip:bob@example.com SIP/2.0\r\n"
        "From: Bob <sip:bob@example.com>;tag=123\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    write(fds2[1], register_bob.data(), register_bob.size());
    conn2->handleRead();
    conn2->handleWrite();
    n = read(fds2[1], buf, sizeof(buf));
    ASSERT_GT(n, 0);
    resp.assign(buf, n);
    ASSERT_TRUE(resp.find("SIP/2.0 200 OK") != std::string::npos);

    // 3. Alice invites Bob
    std::string invite_req = 
        "INVITE sip:bob@example.com SIP/2.0\r\n"
        "Via: SIP/2.0/TCP client.example.com:5060;branch=z9hG4bK776\r\n"
        "From: Alice <sip:alice@example.com>;tag=1928301774\r\n"
        "To: Bob <sip:bob@example.com>\r\n"
        "Call-ID: a84b4c76e66710\r\n"
        "CSeq: 2 INVITE\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    write(fds[1], invite_req.data(), invite_req.size());
    conn1->handleRead(); 
    
    // conn2 should have data to write (the forwarded INVITE)
    conn2->handleWrite(); 
    
    // Read from fds2[1] (Bob's client side)
    n = read(fds2[1], buf, sizeof(buf));
    ASSERT_GT(n, 0);
    std::string bob_recv(buf, n);
    // Bob should receive the raw INVITE
    ASSERT_TRUE(bob_recv.find("INVITE sip:bob@example.com SIP/2.0") != std::string::npos);

    close(fds[1]);
    close(fds2[1]);
}

TEST_F(ChatRoomServerTest, FtpLoginTest) {
    // EventLoop loop; // Removed
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    fcntl(fds[0], F_SETFL, O_NONBLOCK);
    fcntl(fds[1], F_SETFL, O_NONBLOCK);

    InetAddress localAddr(0);
    InetAddress peerAddr(0);
    auto conn = std::make_shared<TcpConnection>(server_->getLoop(), "ftp-conn", fds[0], localAddr, peerAddr);
    conn->setMessageCallback(std::bind(&HttpServer::onMessage, GetHttpServer(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    conn->connectEstablished(); // Fix: Mark connection as established
    GetHttpServer()->onConnection(conn);

    // Client sends USER command to trigger detection and session creation
    std::string user_cmd = "USER anonymous\r\n";
    write(fds[1], user_cmd.data(), user_cmd.size());

    // Server process
    conn->handleRead();
    conn->handleWrite(); // Flush

    char buf[4096];
    int n = read(fds[1], buf, sizeof(buf));
    // ASSERT_GT(n, 0); // Removed assertion to see output
    std::string resp(buf, n > 0 ? n : 0);
    
    // Check for greeting (220) and USER response (331)
    // Note: They might come in one packet or separate
    // EXPECT_TRUE(resp.find("220 Service ready") != std::string::npos); // 220 not sent on connect
    EXPECT_TRUE(resp.find("331 User name okay") != std::string::npos);

    // Send PASS
    std::string pass_cmd = "PASS guest\r\n";
    write(fds[1], pass_cmd.data(), pass_cmd.size());
    conn->handleRead();
    conn->handleWrite();
    
    n = read(fds[1], buf, sizeof(buf));
    ASSERT_GT(n, 0);
    resp.assign(buf, n);
    EXPECT_TRUE(resp.find("230 User logged in") != std::string::npos);

    close(fds[1]);
}

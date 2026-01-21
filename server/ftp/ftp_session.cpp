#include "ftp/ftp_session.h"
#include "net/tcp_connection.h"
#include "http/http_server.h"
#include "logger.h"

FtpSession::FtpSession(HttpServer* server, std::shared_ptr<TcpConnection> conn)
    : server_(server), conn_(conn) {
    // Send initial greeting
    // Note: This might be too late if TcpConnection is already established and client is waiting?
    // But TcpConnection creates session upon first data or manually?
    // Wait, TcpConnection creates session in ensureSession() which is called onData.
    // So for FTP, if client doesn't send anything, we never create session.
    // We'll address this in protocol detection.
    if (auto c = conn_.lock()) {
        c->send("220 Service ready for new user.\r\n");
    }
}

FtpSession::~FtpSession() {
}

void FtpSession::onData(const char* data, std::size_t len) {
    buffer_.append(data, len);
    
    size_t pos = buffer_.find("\r\n");
    while (pos != std::string::npos) {
        std::string command = buffer_.substr(0, pos);
        buffer_.erase(0, pos + 2);
        handleCommand(command);
        pos = buffer_.find("\r\n");
    }
}

void FtpSession::handleCommand(const std::string& command) {
    LOG_INFO("Received FTP command: {}", command);
    
    if (server_) {
        if (auto conn = conn_.lock()) {
            server_->handleFtpMessage(conn, command);
            return;
        }
    }

    // Simple echo/mock response
    std::string response = "502 Command not implemented.\r\n";
    if (command.find("USER") == 0) {
        response = "331 User name okay, need password.\r\n";
    } else if (command.find("PASS") == 0) {
        response = "230 User logged in, proceed.\r\n";
    } else if (command.find("QUIT") == 0) {
        response = "221 Service closing control connection.\r\n";
        if (auto conn = conn_.lock()) {
            conn->send(response);
            conn->setCloseAfterWrite(true);
        }
        return;
    }

    if (auto conn = conn_.lock()) {
        conn->send(response);
    }
}

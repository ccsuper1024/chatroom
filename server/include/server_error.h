#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "http_codec.h"

using json = nlohmann::json;

enum class ErrorCode {
    SUCCESS = 0,
    INVALID_REQUEST = 1001,
    INVALID_USERNAME = 1002,
    INVALID_MESSAGE = 1003,
    RATE_LIMITED = 1004,
    USERNAME_TAKEN = 1005,
    PAYLOAD_TOO_LARGE = 1006,
    SERVER_BUSY = 1007,
    INTERNAL_ERROR = 5000,
    UNKNOWN_ERROR = 9999
};

struct AppError {
    ErrorCode code;
    std::string message;
    int http_status_code;

    static AppError FromErrorCode(ErrorCode code) {
        switch (code) {
            case ErrorCode::SUCCESS:
                return {ErrorCode::SUCCESS, "Success", 200};
            case ErrorCode::INVALID_REQUEST:
                return {ErrorCode::INVALID_REQUEST, "Invalid request format", 400};
            case ErrorCode::INVALID_USERNAME:
                return {ErrorCode::INVALID_USERNAME, "Invalid username (1-32 chars, alphanumeric and underscore only)", 400};
            case ErrorCode::INVALID_MESSAGE:
                return {ErrorCode::INVALID_MESSAGE, "Invalid message content (1-1024 chars, no control chars)", 400};
            case ErrorCode::RATE_LIMITED:
                return {ErrorCode::RATE_LIMITED, "Too Many Requests", 429};
            case ErrorCode::USERNAME_TAKEN:
                return {ErrorCode::USERNAME_TAKEN, "Username already taken", 409};
            case ErrorCode::PAYLOAD_TOO_LARGE:
                return {ErrorCode::PAYLOAD_TOO_LARGE, "Request entity too large", 413};
            case ErrorCode::SERVER_BUSY:
                return {ErrorCode::SERVER_BUSY, "Server Busy, try again later", 503};
            case ErrorCode::INTERNAL_ERROR:
                return {ErrorCode::INTERNAL_ERROR, "Internal Server Error", 500};
            default:
                return {ErrorCode::UNKNOWN_ERROR, "Unknown Error", 500};
        }
    }
};

inline HttpResponse CreateErrorResponse(ErrorCode code, const std::string& custom_msg = "") {
    AppError error = AppError::FromErrorCode(code);
    HttpResponse response;
    response.status_code = error.http_status_code;
    response.status_text = (code == ErrorCode::SUCCESS) ? "OK" : "Error";
    
    json body;
    body["success"] = false;
    body["error_code"] = static_cast<int>(error.code);
    body["error"] = custom_msg.empty() ? error.message : custom_msg;
    
    response.body = body.dump();
    return response;
}

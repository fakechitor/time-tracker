#pragma once

#include <optional>
#include <string>

namespace app::auth {

struct User {
    std::string id;
    std::string username;
    std::string email;
    std::string password_hash;
};

struct LoginRequest {
    std::string email;
    std::string password;
};

struct RegisterRequest {
    std::string username;
    std::string email;
    std::string password;
    std::string confirmed_password;
};

struct RefreshRequest {
    std::string refresh_token;
};

struct TokenPair {
    std::string access_token;
    std::string refresh_token;
};

struct TokenClaims {
    std::string user_id;
    std::string email;
    std::string token_type;
    std::string token_id;
    long long issued_at{};
    long long expires_at{};
};

struct ServiceError {
    int status_code{};
    std::string message;
};

template <typename T>
struct ServiceResult {
    std::optional<T> value;
    std::optional<ServiceError> error;

    static ServiceResult ok(T payload) {
        ServiceResult result;
        result.value = std::move(payload);
        return result;
    }

    static ServiceResult fail(int code, std::string message) {
        ServiceResult result;
        result.error = ServiceError{code, std::move(message)};
        return result;
    }
};

}  // namespace app::auth


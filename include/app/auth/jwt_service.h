#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "app/auth/models.h"

namespace app::auth {

class JwtService {
public:
    JwtService(std::string secret, std::string issuer, std::string audience, long long access_ttl_seconds, long long refresh_ttl_seconds);

    TokenPair createTokenPair(const User& user);
    ServiceResult<TokenPair> rotateByRefreshToken(const std::string& refresh_token);
    ServiceResult<TokenClaims> verifyAccessToken(const std::string& access_token) const;

private:
    struct RefreshSession {
        std::string user_id;
        long long expires_at{};
    };

    static long long unixNow();

    std::string createToken(const TokenClaims& claims) const;
    ServiceResult<TokenClaims> verifyToken(const std::string& token, const std::string& expected_type) const;

    std::string base64UrlNoPadding(const std::string& input) const;
    std::string base64UrlDecode(const std::string& input) const;
    std::string signHs256(const std::string& message) const;
    std::string randomTokenId() const;

    std::string secret_;
    std::string issuer_;
    std::string audience_;
    long long access_ttl_seconds_{};
    long long refresh_ttl_seconds_{};

    mutable std::mutex refresh_mutex_;
    std::unordered_map<std::string, RefreshSession> refresh_sessions_;
};

}  // namespace app::auth


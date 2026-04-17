#include "app/auth/jwt_service.h"

#include <array>
#include <chrono>
#include <random>
#include <sstream>
#include <string_view>
#include <vector>

#include <crow.h>
#include <picosha2.h>

namespace app::auth {

namespace {

std::vector<unsigned char> sha256Bytes(const std::vector<unsigned char>& input) {
    std::vector<unsigned char> hash(picosha2::k_digest_size);
    picosha2::hash256(input.begin(), input.end(), hash.begin(), hash.end());
    return hash;
}

std::vector<unsigned char> hmacSha256(const std::string& key, const std::string& message) {
    constexpr std::size_t block_size = 64;
    std::vector<unsigned char> key_bytes(key.begin(), key.end());
    if (key_bytes.size() > block_size) {
        key_bytes = sha256Bytes(key_bytes);
    }
    key_bytes.resize(block_size, 0x00);

    std::vector<unsigned char> o_key_pad(block_size, 0x5c);
    std::vector<unsigned char> i_key_pad(block_size, 0x36);

    for (std::size_t i = 0; i < block_size; ++i) {
        o_key_pad[i] ^= key_bytes[i];
        i_key_pad[i] ^= key_bytes[i];
    }

    std::vector<unsigned char> inner(i_key_pad.begin(), i_key_pad.end());
    inner.insert(inner.end(), message.begin(), message.end());
    const std::vector<unsigned char> inner_hash = sha256Bytes(inner);

    std::vector<unsigned char> outer(o_key_pad.begin(), o_key_pad.end());
    outer.insert(outer.end(), inner_hash.begin(), inner_hash.end());
    return sha256Bytes(outer);
}

std::vector<std::string_view> splitToken(std::string_view token) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i < token.size(); ++i) {
        if (token[i] == '.') {
            parts.push_back(token.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.push_back(token.substr(start));
    return parts;
}

bool constantTimeEquals(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        diff |= static_cast<unsigned char>(left[i] ^ right[i]);
    }
    return diff == 0;
}

}  // namespace

JwtService::JwtService(std::string secret, std::string issuer, std::string audience, long long access_ttl_seconds, long long refresh_ttl_seconds)
    : secret_(std::move(secret)),
      issuer_(std::move(issuer)),
      audience_(std::move(audience)),
      access_ttl_seconds_(access_ttl_seconds),
      refresh_ttl_seconds_(refresh_ttl_seconds) {}

long long JwtService::unixNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

TokenPair JwtService::createTokenPair(const User& user) {
    const long long now = unixNow();

    TokenClaims access_claims;
    access_claims.user_id = user.id;
    access_claims.email = user.email;
    access_claims.token_type = "access";
    access_claims.token_id = randomTokenId();
    access_claims.issued_at = now;
    access_claims.expires_at = now + access_ttl_seconds_;

    TokenClaims refresh_claims;
    refresh_claims.user_id = user.id;
    refresh_claims.email = user.email;
    refresh_claims.token_type = "refresh";
    refresh_claims.token_id = randomTokenId();
    refresh_claims.issued_at = now;
    refresh_claims.expires_at = now + refresh_ttl_seconds_;

    {
        std::lock_guard<std::mutex> lock(refresh_mutex_);
        refresh_sessions_[refresh_claims.token_id] = RefreshSession{
            .user_id = user.id,
            .expires_at = refresh_claims.expires_at,
        };
    }

    return TokenPair{
        .access_token = createToken(access_claims),
        .refresh_token = createToken(refresh_claims),
    };
}

ServiceResult<TokenPair> JwtService::rotateByRefreshToken(const std::string& refresh_token) {
    const auto verified = verifyToken(refresh_token, "refresh");
    if (verified.error.has_value()) {
        return ServiceResult<TokenPair>::fail(401, verified.error->message);
    }

    const TokenClaims& claims = *verified.value;
    User temp_user{
        .id = claims.user_id,
        .username = "",
        .email = claims.email,
        .password_hash = "",
    };

    {
        std::lock_guard<std::mutex> lock(refresh_mutex_);
        const auto it = refresh_sessions_.find(claims.token_id);
        if (it == refresh_sessions_.end()) {
            return ServiceResult<TokenPair>::fail(401, "Refresh token is revoked");
        }
        if (it->second.user_id != claims.user_id || it->second.expires_at < unixNow()) {
            refresh_sessions_.erase(it);
            return ServiceResult<TokenPair>::fail(401, "Refresh token is expired");
        }
        refresh_sessions_.erase(it);
    }

    return ServiceResult<TokenPair>::ok(createTokenPair(temp_user));
}

ServiceResult<TokenClaims> JwtService::verifyAccessToken(const std::string& access_token) const {
    return verifyToken(access_token, "access");
}

ServiceResult<TokenClaims> JwtService::verifyToken(const std::string& token, const std::string& expected_type) const {
    const auto parts = splitToken(token);
    if (parts.size() != 3) {
        return ServiceResult<TokenClaims>::fail(401, "Malformed token");
    }

    const std::string header_payload = std::string(parts[0]) + "." + std::string(parts[1]);
    const std::string expected_signature = signHs256(header_payload);
    if (!constantTimeEquals(expected_signature, std::string(parts[2]))) {
        return ServiceResult<TokenClaims>::fail(401, "Invalid token signature");
    }

    const std::string header_json = base64UrlDecode(std::string(parts[0]));
    const std::string payload_json = base64UrlDecode(std::string(parts[1]));
    const auto header = crow::json::load(header_json);
    const auto payload = crow::json::load(payload_json);
    if (!header || !payload) {
        return ServiceResult<TokenClaims>::fail(401, "Invalid token payload");
    }

    if (!header.has("alg") || std::string(header["alg"].s()) != "HS256") {
        return ServiceResult<TokenClaims>::fail(401, "Unsupported token algorithm");
    }

    if (!payload.has("iss") || !payload.has("aud") || !payload.has("sub") || !payload.has("email") ||
        !payload.has("type") || !payload.has("jti") || !payload.has("iat") || !payload.has("exp")) {
        return ServiceResult<TokenClaims>::fail(401, "Missing token claims");
    }

    if (std::string(payload["iss"].s()) != issuer_ || std::string(payload["aud"].s()) != audience_) {
        return ServiceResult<TokenClaims>::fail(401, "Invalid token issuer or audience");
    }

    const long long now = unixNow();
    const long long expires_at = payload["exp"].i();
    const long long issued_at = payload["iat"].i();
    if (expires_at <= now || issued_at > now) {
        return ServiceResult<TokenClaims>::fail(401, "Token is expired");
    }

    const std::string token_type = payload["type"].s();
    if (token_type != expected_type) {
        return ServiceResult<TokenClaims>::fail(401, "Unexpected token type");
    }

    TokenClaims claims;
    claims.user_id = payload["sub"].s();
    claims.email = payload["email"].s();
    claims.token_type = token_type;
    claims.token_id = payload["jti"].s();
    claims.issued_at = issued_at;
    claims.expires_at = expires_at;

    return ServiceResult<TokenClaims>::ok(claims);
}

std::string JwtService::createToken(const TokenClaims& claims) const {
    crow::json::wvalue header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";

    crow::json::wvalue payload;
    payload["iss"] = issuer_;
    payload["aud"] = audience_;
    payload["sub"] = claims.user_id;
    payload["email"] = claims.email;
    payload["type"] = claims.token_type;
    payload["jti"] = claims.token_id;
    payload["iat"] = claims.issued_at;
    payload["exp"] = claims.expires_at;

    const std::string encoded_header = base64UrlNoPadding(header.dump());
    const std::string encoded_payload = base64UrlNoPadding(payload.dump());
    const std::string header_payload = encoded_header + "." + encoded_payload;
    const std::string signature = signHs256(header_payload);

    return header_payload + "." + signature;
}

std::string JwtService::base64UrlNoPadding(const std::string& input) const {
    std::string encoded = crow::utility::base64encode_urlsafe(input, input.size());
    while (!encoded.empty() && encoded.back() == '=') {
        encoded.pop_back();
    }
    return encoded;
}

std::string JwtService::base64UrlDecode(const std::string& input) const {
    std::string padded = input;
    while ((padded.size() % 4) != 0) {
        padded.push_back('=');
    }
    return crow::utility::base64decode(padded);
}

std::string JwtService::signHs256(const std::string& message) const {
    const std::vector<unsigned char> digest = hmacSha256(secret_, message);
    std::string raw;
    raw.reserve(digest.size());
    for (unsigned char byte : digest) {
        raw.push_back(static_cast<char>(byte));
    }
    return base64UrlNoPadding(raw);
}

std::string JwtService::randomTokenId() const {
    static thread_local std::mt19937_64 generator(std::random_device{}());
    static constexpr std::array<char, 16> alphabet = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::uniform_int_distribution<int> dist(0, static_cast<int>(alphabet.size() - 1));

    std::string token_id;
    token_id.reserve(32);
    for (int i = 0; i < 32; ++i) {
        token_id.push_back(alphabet[dist(generator)]);
    }
    return token_id;
}

}  // namespace app::auth


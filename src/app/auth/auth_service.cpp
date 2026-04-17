#include "app/auth/auth_service.h"

#include <vector>

#include <crow.h>
#include <picosha2.h>

namespace app::auth {

AuthService::AuthService(UserStore& user_store, JwtService& jwt_service)
    : user_store_(user_store), jwt_service_(jwt_service) {}

ServiceResult<TokenPair> AuthService::registerUser(const RegisterRequest& request) {
    if (request.username.empty() || request.email.empty() || request.password.empty()) {
        return ServiceResult<TokenPair>::fail(400, "username, email and password are required");
    }
    if (request.password != request.confirmed_password) {
        return ServiceResult<TokenPair>::fail(400, "password and confirmed_password must match");
    }
    if (user_store_.existsByEmail(request.email)) {
        return ServiceResult<TokenPair>::fail(409, "user with this email already exists");
    }

    User user = user_store_.create(request.username, request.email, hashPassword(request.password));
    return ServiceResult<TokenPair>::ok(jwt_service_.createTokenPair(user));
}

ServiceResult<TokenPair> AuthService::login(const LoginRequest& request) {
    if (request.email.empty() || request.password.empty()) {
        return ServiceResult<TokenPair>::fail(400, "email and password are required");
    }

    const auto user = user_store_.findByEmail(request.email);
    if (!user.has_value()) {
        return ServiceResult<TokenPair>::fail(401, "invalid credentials");
    }

    if (user->password_hash != hashPassword(request.password)) {
        return ServiceResult<TokenPair>::fail(401, "invalid credentials");
    }

    return ServiceResult<TokenPair>::ok(jwt_service_.createTokenPair(*user));
}

ServiceResult<TokenPair> AuthService::refresh(const RefreshRequest& request) {
    if (request.refresh_token.empty()) {
        return ServiceResult<TokenPair>::fail(400, "refresh_token is required");
    }
    return jwt_service_.rotateByRefreshToken(request.refresh_token);
}

ServiceResult<User> AuthService::getUserById(const std::string& user_id) {
    const auto user = user_store_.findById(user_id);
    if (!user.has_value()) {
        return ServiceResult<User>::fail(404, "user not found");
    }
    return ServiceResult<User>::ok(*user);
}

std::string AuthService::hashPassword(const std::string& password) const {
    std::vector<unsigned char> digest(picosha2::k_digest_size);
    picosha2::hash256(password.begin(), password.end(), digest.begin(), digest.end());
    return picosha2::bytes_to_hex_string(digest.begin(), digest.end());
}

}  // namespace app::auth


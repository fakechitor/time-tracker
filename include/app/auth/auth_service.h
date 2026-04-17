#pragma once

#include <string>

#include "app/auth/jwt_service.h"
#include "app/auth/models.h"
#include "app/auth/user_store.h"

namespace app::auth {

class AuthService {
public:
    AuthService(UserStore& user_store, JwtService& jwt_service);

    ServiceResult<TokenPair> registerUser(const RegisterRequest& request);
    ServiceResult<TokenPair> login(const LoginRequest& request);
    ServiceResult<TokenPair> refresh(const RefreshRequest& request);
    ServiceResult<User> getUserById(const std::string& user_id);

private:
    std::string hashPassword(const std::string& password) const;

    UserStore& user_store_;
    JwtService& jwt_service_;
};

}  // namespace app::auth


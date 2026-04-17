#pragma once

#include <memory>
#include <string>

#include <crow.h>

#include "app/auth/jwt_service.h"

namespace app::auth {

class AuthMiddleware {
public:
    struct context {
        bool authenticated{false};
        std::string user_id;
        std::string email;
    };

    static void configure(std::shared_ptr<JwtService> jwt_service);

    void before_handle(crow::request& req, crow::response& res, context& ctx);
    void after_handle(crow::request&, crow::response&, context&) {}

private:
    static bool isPublicRoute(const std::string& url);
    static bool needsAuth(const std::string& url);

    static std::shared_ptr<JwtService> jwt_service_;
};

}  // namespace app::auth


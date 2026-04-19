#include "app/auth/auth_middleware.h"

#include <algorithm>

#include "app/http/json_response.h"

namespace app::auth {

std::shared_ptr<JwtService> AuthMiddleware::jwt_service_;

void AuthMiddleware::configure(std::shared_ptr<JwtService> jwt_service) {
    jwt_service_ = std::move(jwt_service);
}

bool AuthMiddleware::isPublicRoute(const std::string& url) {
    return url == "/" ||
           url == "/health" ||
           url == "/api/v1/auth/login" ||
           url == "/api/v1/auth/register" ||
           url == "/api/v1/auth/refresh";
}

bool AuthMiddleware::needsAuth(const std::string& url) {
    return url.rfind("/api/v1/", 0) == 0 && !isPublicRoute(url);
}

void AuthMiddleware::before_handle(crow::request& req, crow::response& res, context& ctx) {
    if (req.method == crow::HTTPMethod::Options) {
        return;
    }

    if (!needsAuth(req.url)) {
        return;
    }
    if (!jwt_service_) {
        res = http::error(500, "Auth middleware is not configured");
        res.end();
        return;
    }

    const std::string auth_header = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (auth_header.rfind(prefix, 0) != 0) {
        res = http::error(401, "Authorization header must be Bearer token");
        res.end();
        return;
    }

    const std::string token = auth_header.substr(prefix.size());
    auto verified = jwt_service_->verifyAccessToken(token);
    if (verified.error.has_value()) {
        res = http::error(401, verified.error->message);
        res.end();
        return;
    }

    ctx.authenticated = true;
    ctx.user_id = verified.value->user_id;
    ctx.email = verified.value->email;
}

}  // namespace app::auth

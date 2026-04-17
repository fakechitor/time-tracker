#include "app/routes/auth_routes.h"

#include "app/http/json_response.h"

namespace app::routes {

namespace {

auth::ServiceResult<auth::LoginRequest> parseLogin(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body) {
        return auth::ServiceResult<auth::LoginRequest>::fail(400, "Invalid JSON");
    }
    if (!body.has("email") || !body.has("password")) {
        return auth::ServiceResult<auth::LoginRequest>::fail(400, "email and password are required");
    }
    return auth::ServiceResult<auth::LoginRequest>::ok(auth::LoginRequest{
        .email = body["email"].s(),
        .password = body["password"].s(),
    });
}

auth::ServiceResult<auth::RegisterRequest> parseRegister(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body) {
        return auth::ServiceResult<auth::RegisterRequest>::fail(400, "Invalid JSON");
    }
    if (!body.has("username") || !body.has("email") || !body.has("password") || !body.has("confirmed_password")) {
        return auth::ServiceResult<auth::RegisterRequest>::fail(400, "username, email, password and confirmed_password are required");
    }
    return auth::ServiceResult<auth::RegisterRequest>::ok(auth::RegisterRequest{
        .username = body["username"].s(),
        .email = body["email"].s(),
        .password = body["password"].s(),
        .confirmed_password = body["confirmed_password"].s(),
    });
}

auth::ServiceResult<auth::RefreshRequest> parseRefresh(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body) {
        return auth::ServiceResult<auth::RefreshRequest>::fail(400, "Invalid JSON");
    }
    if (!body.has("refresh_token")) {
        return auth::ServiceResult<auth::RefreshRequest>::fail(400, "refresh_token is required");
    }
    return auth::ServiceResult<auth::RefreshRequest>::ok(auth::RefreshRequest{
        .refresh_token = body["refresh_token"].s(),
    });
}

crow::response tokenResponse(int status, const auth::TokenPair& token_pair) {
    crow::json::wvalue body;
    body["access_token"] = token_pair.access_token;
    body["refresh_token"] = token_pair.refresh_token;
    return http::json(status, std::move(body));
}

}  // namespace

void registerAuthRoutes(HttpApp& app, auth::AuthService& auth_service) {
    CROW_ROUTE(app, "/api/v1/auth/login")
        .methods(crow::HTTPMethod::Post)([&auth_service](const crow::request& request) {
            auto parsed = parseLogin(request);
            if (parsed.error.has_value()) {
                return http::error(parsed.error->status_code, parsed.error->message);
            }

            auto result = auth_service.login(*parsed.value);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }

            return tokenResponse(200, *result.value);
        });

    CROW_ROUTE(app, "/api/v1/auth/register")
        .methods(crow::HTTPMethod::Post)([&auth_service](const crow::request& request) {
            auto parsed = parseRegister(request);
            if (parsed.error.has_value()) {
                return http::error(parsed.error->status_code, parsed.error->message);
            }

            auto result = auth_service.registerUser(*parsed.value);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }

            return tokenResponse(201, *result.value);
        });

    CROW_ROUTE(app, "/api/v1/auth/refresh")
        .methods(crow::HTTPMethod::Post)([&auth_service](const crow::request& request) {
            auto parsed = parseRefresh(request);
            if (parsed.error.has_value()) {
                return http::error(parsed.error->status_code, parsed.error->message);
            }

            auto result = auth_service.refresh(*parsed.value);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }

            return tokenResponse(200, *result.value);
        });
}

}  // namespace app::routes


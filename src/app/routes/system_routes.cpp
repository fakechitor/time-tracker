#include "app/routes/system_routes.h"

#include "app/http/json_response.h"

namespace app::routes {

void registerSystemRoutes(HttpApp& app, auth::AuthService& auth_service) {
    CROW_ROUTE(app, "/")([] {
        crow::json::wvalue body;
        body["name"] = "time-tracker-app";
        body["status"] = "running";
        return http::json(200, std::move(body));
    });

    CROW_ROUTE(app, "/health")([] {
        crow::json::wvalue body;
        body["status"] = "ok";
        return http::json(200, std::move(body));
    });

    CROW_ROUTE(app, "/api/v1/auth/me")
    ([&app, &auth_service](const crow::request& request) {
        const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
        auto user_result = auth_service.getUserById(auth_ctx.user_id);
        if (user_result.error.has_value()) {
            return http::error(user_result.error->status_code, user_result.error->message);
        }

        crow::json::wvalue body;
        body["id"] = user_result.value->id;
        body["username"] = user_result.value->username;
        body["email"] = user_result.value->email;
        return http::json(200, std::move(body));
    });
}

}  // namespace app::routes


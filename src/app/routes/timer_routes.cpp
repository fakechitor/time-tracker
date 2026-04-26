#include "app/routes/timer_routes.h"

#include <string>

#include "app/auth/auth_middleware.h"
#include "app/http/json_response.h"

namespace app::routes {
namespace {

auth::ServiceResult<timers::StartTimerRequest> parseStart(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body) {
        return auth::ServiceResult<timers::StartTimerRequest>::fail(400, "Invalid JSON");
    }
    if (!body.has("task_id")) {
        return auth::ServiceResult<timers::StartTimerRequest>::fail(400, "task_id is required");
    }
    return auth::ServiceResult<timers::StartTimerRequest>::ok(timers::StartTimerRequest{
        .task_id = body["task_id"].s(),
    });
}

auth::ServiceResult<timers::StopTimerRequest> parseStop(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body) {
        return auth::ServiceResult<timers::StopTimerRequest>::ok(timers::StopTimerRequest{});
    }
    return auth::ServiceResult<timers::StopTimerRequest>::ok(timers::StopTimerRequest{
        .task_id = body.has("task_id") ? std::string(body["task_id"].s()) : "",
    });
}

crow::json::wvalue toJson(const timers::TimerSession& session) {
    crow::json::wvalue body;
    body["id"] = session.id;
    body["task_id"] = session.task_id;
    body["user_id"] = session.user_id;
    body["started_at"] = session.started_at;
    body["stopped_at"] = session.stopped_at;
    body["is_active"] = session.is_active;
    body["created_at"] = session.created_at;
    body["updated_at"] = session.updated_at;
    return body;
}

}  // namespace

void registerTimerRoutes(HttpApp& app, timers::TimerService& service) {
    CROW_ROUTE(app, "/api/v1/timers/start")
        .methods(crow::HTTPMethod::Post)([&app, &service](const crow::request& request) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            auto parsed = parseStart(request);
            if (parsed.error.has_value()) {
                return http::error(parsed.error->status_code, parsed.error->message);
            }

            auto result = service.startTimer(auth_ctx.user_id, *parsed.value);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }
            return http::json(200, toJson(*result.value));
        });

    CROW_ROUTE(app, "/api/v1/timers/stop")
        .methods(crow::HTTPMethod::Post)([&app, &service](const crow::request& request) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            auto parsed = parseStop(request);
            if (parsed.error.has_value()) {
                return http::error(parsed.error->status_code, parsed.error->message);
            }

            auto result = service.stopTimer(auth_ctx.user_id, *parsed.value);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }
            return http::json(200, toJson(*result.value));
        });

    CROW_ROUTE(app, "/api/v1/timers/active")
        .methods(crow::HTTPMethod::Get)([&app, &service](const crow::request& request) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            auto result = service.getActiveTimer(auth_ctx.user_id);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }
            return http::json(200, toJson(*result.value));
        });
}

}  // namespace app::routes

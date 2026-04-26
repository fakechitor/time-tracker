#include "app/routes/time_entry_routes.h"

#include <string>
#include <utility>

#include "app/auth/auth_middleware.h"
#include "app/http/json_response.h"

namespace app::routes {
namespace {

auth::ServiceResult<time_entries::CreateTimeEntryRequest> parseCreate(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body) {
        return auth::ServiceResult<time_entries::CreateTimeEntryRequest>::fail(400, "Invalid JSON");
    }
    if (!body.has("task_id") || !body.has("started_at") || !body.has("ended_at")) {
        return auth::ServiceResult<time_entries::CreateTimeEntryRequest>::fail(
            400, "task_id, started_at and ended_at are required");
    }

    return auth::ServiceResult<time_entries::CreateTimeEntryRequest>::ok(time_entries::CreateTimeEntryRequest{
        .task_id = body["task_id"].s(),
        .started_at = body["started_at"].s(),
        .ended_at = body["ended_at"].s(),
    });
}

auth::ServiceResult<time_entries::UpdateTimeEntryRequest> parseUpdate(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body) {
        return auth::ServiceResult<time_entries::UpdateTimeEntryRequest>::fail(400, "Invalid JSON");
    }
    if (!body.has("task_id") || !body.has("started_at") || !body.has("ended_at")) {
        return auth::ServiceResult<time_entries::UpdateTimeEntryRequest>::fail(
            400, "task_id, started_at and ended_at are required");
    }

    return auth::ServiceResult<time_entries::UpdateTimeEntryRequest>::ok(time_entries::UpdateTimeEntryRequest{
        .task_id = body["task_id"].s(),
        .started_at = body["started_at"].s(),
        .ended_at = body["ended_at"].s(),
    });
}

time_entries::ListTimeEntriesQuery parseListQuery(const crow::request& request) {
    time_entries::ListTimeEntriesQuery query;
    if (const char* task_id = request.url_params.get("task_id")) {
        query.task_id = task_id;
    }
    if (const char* from = request.url_params.get("from")) {
        query.from = from;
    }
    if (const char* to = request.url_params.get("to")) {
        query.to = to;
    }
    if (const char* sort_by = request.url_params.get("sort_by")) {
        query.sort_by = sort_by;
    }
    if (const char* order = request.url_params.get("order")) {
        query.order = order;
    }
    if (const char* limit = request.url_params.get("limit")) {
        try {
            query.limit = std::stoi(limit);
        } catch (...) {
            query.limit = 50;
        }
    }
    if (const char* offset = request.url_params.get("offset")) {
        try {
            query.offset = std::stoi(offset);
        } catch (...) {
            query.offset = 0;
        }
    }
    return query;
}

crow::json::wvalue timeEntryToJson(const time_entries::TimeEntry& entry) {
    crow::json::wvalue body;
    body["id"] = entry.id;
    body["task_id"] = entry.task_id;
    body["user_id"] = entry.user_id;
    body["started_at"] = entry.started_at;
    body["ended_at"] = entry.ended_at;
    body["duration_sec"] = entry.duration_sec;
    body["created_at"] = entry.created_at;
    body["updated_at"] = entry.updated_at;
    return body;
}

}  // namespace

void registerTimeEntryRoutes(HttpApp& app, time_entries::TimeEntryService& service) {
    CROW_ROUTE(app, "/api/v1/time-entries")
        .methods(crow::HTTPMethod::Post)([&app, &service](const crow::request& request) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            auto parsed = parseCreate(request);
            if (parsed.error.has_value()) {
                return http::error(parsed.error->status_code, parsed.error->message);
            }

            auto result = service.createTimeEntry(auth_ctx.user_id, *parsed.value);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }
            return http::json(201, timeEntryToJson(*result.value));
        });

    CROW_ROUTE(app, "/api/v1/time-entries")
        .methods(crow::HTTPMethod::Get)([&app, &service](const crow::request& request) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            auto query = parseListQuery(request);
            auto result = service.listTimeEntries(auth_ctx.user_id, query);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }

            crow::json::wvalue::list items;
            items.reserve(result.value->size());
            for (const auto& entry : *result.value) {
                items.push_back(timeEntryToJson(entry));
            }

            crow::json::wvalue body;
            body["time_entries"] = std::move(items);
            body["count"] = static_cast<int>(result.value->size());
            return http::json(200, std::move(body));
        });

    CROW_ROUTE(app, "/api/v1/time-entries/<string>")
        .methods(crow::HTTPMethod::Get)([&app, &service](const crow::request& request, const std::string& entry_id) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            auto result = service.getTimeEntry(auth_ctx.user_id, entry_id);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }
            return http::json(200, timeEntryToJson(*result.value));
        });

    CROW_ROUTE(app, "/api/v1/time-entries/<string>")
        .methods(crow::HTTPMethod::Put)([&app, &service](const crow::request& request, const std::string& entry_id) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            auto parsed = parseUpdate(request);
            if (parsed.error.has_value()) {
                return http::error(parsed.error->status_code, parsed.error->message);
            }

            auto result = service.updateTimeEntry(auth_ctx.user_id, entry_id, *parsed.value);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }
            return http::json(200, timeEntryToJson(*result.value));
        });

    CROW_ROUTE(app, "/api/v1/time-entries/<string>")
        .methods(crow::HTTPMethod::Delete)([&app, &service](const crow::request& request, const std::string& entry_id) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            auto result = service.deleteTimeEntry(auth_ctx.user_id, entry_id);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }

            crow::json::wvalue body;
            body["deleted"] = true;
            return http::json(200, std::move(body));
        });
}

}  // namespace app::routes

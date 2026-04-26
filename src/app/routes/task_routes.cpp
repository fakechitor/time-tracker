#include "app/routes/task_routes.h"

#include <string>

#include "app/auth/auth_middleware.h"
#include "app/http/json_response.h"

namespace app::routes {
namespace {

auth::ServiceResult<tasks::CreateTaskRequest> parseCreate(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body) {
        return auth::ServiceResult<tasks::CreateTaskRequest>::fail(400, "Invalid JSON");
    }
    if (!body.has("title")) {
        return auth::ServiceResult<tasks::CreateTaskRequest>::fail(400, "title is required");
    }
    return auth::ServiceResult<tasks::CreateTaskRequest>::ok(tasks::CreateTaskRequest{
        .title = body["title"].s(),
        .description = body.has("description") ? std::string(body["description"].s()) : "",
        .status = body.has("status") ? std::string(body["status"].s()) : "active",
    });
}

auth::ServiceResult<tasks::UpdateTaskRequest> parseUpdate(const crow::request& request) {
    auto body = crow::json::load(request.body);
    if (!body) {
        return auth::ServiceResult<tasks::UpdateTaskRequest>::fail(400, "Invalid JSON");
    }
    if (!body.has("title") || !body.has("status")) {
        return auth::ServiceResult<tasks::UpdateTaskRequest>::fail(400, "title and status are required");
    }
    return auth::ServiceResult<tasks::UpdateTaskRequest>::ok(tasks::UpdateTaskRequest{
        .title = body["title"].s(),
        .description = body.has("description") ? std::string(body["description"].s()) : "",
        .status = body["status"].s(),
    });
}

tasks::ListTasksQuery parseListQuery(const crow::request& request) {
    tasks::ListTasksQuery query;
    if (const char* search = request.url_params.get("search")) {
        query.search = search;
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

crow::json::wvalue taskToJson(const tasks::Task& task) {
    crow::json::wvalue body;
    body["id"] = task.id;
    body["user_id"] = task.user_id;
    body["title"] = task.title;
    body["description"] = task.description;
    body["status"] = task.status;
    body["total_time"] = task.total_time;
    body["created_at"] = task.created_at;
    body["updated_at"] = task.updated_at;
    return body;
}

}  // namespace

void registerTaskRoutes(HttpApp& app, tasks::TaskService& task_service) {
    CROW_ROUTE(app, "/api/v1/tasks")
        .methods(crow::HTTPMethod::Post)([&app, &task_service](const crow::request& request) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);

            auto parsed = parseCreate(request);
            if (parsed.error.has_value()) {
                return http::error(parsed.error->status_code, parsed.error->message);
            }

            auto result = task_service.createTask(auth_ctx.user_id, *parsed.value);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }

            return http::json(201, taskToJson(*result.value));
        });

    CROW_ROUTE(app, "/api/v1/tasks")
        .methods(crow::HTTPMethod::Get)([&app, &task_service](const crow::request& request) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            const tasks::ListTasksQuery query = parseListQuery(request);

            auto result = task_service.listTasks(auth_ctx.user_id, query);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }

            crow::json::wvalue::list items;
            items.reserve(result.value->size());
            for (const auto& task : *result.value) {
                items.push_back(taskToJson(task));
            }

            crow::json::wvalue body;
            body["tasks"] = std::move(items);
            body["count"] = static_cast<int>(result.value->size());
            return http::json(200, std::move(body));
        });

    CROW_ROUTE(app, "/api/v1/tasks/<string>")
        .methods(crow::HTTPMethod::Get)([&app, &task_service](const crow::request& request, const std::string& task_id) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            auto result = task_service.getTask(auth_ctx.user_id, task_id);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }
            return http::json(200, taskToJson(*result.value));
        });

    CROW_ROUTE(app, "/api/v1/tasks/<string>")
        .methods(crow::HTTPMethod::Put)([&app, &task_service](const crow::request& request, const std::string& task_id) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            auto parsed = parseUpdate(request);
            if (parsed.error.has_value()) {
                return http::error(parsed.error->status_code, parsed.error->message);
            }

            auto result = task_service.updateTask(auth_ctx.user_id, task_id, *parsed.value);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }
            return http::json(200, taskToJson(*result.value));
        });

    CROW_ROUTE(app, "/api/v1/tasks/<string>")
        .methods(crow::HTTPMethod::Delete)([&app, &task_service](const crow::request& request, const std::string& task_id) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);
            auto result = task_service.deleteTask(auth_ctx.user_id, task_id);
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }

            crow::json::wvalue body;
            body["deleted"] = true;
            return http::json(200, std::move(body));
        });
}

}  // namespace app::routes

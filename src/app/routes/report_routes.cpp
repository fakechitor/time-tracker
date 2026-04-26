#include "app/routes/report_routes.h"

#include <string>
#include <utility>

#include "app/auth/auth_middleware.h"
#include "app/http/json_response.h"

namespace app::routes {
namespace {

crow::json::wvalue toJson(const reports::SummaryReport& report) {
    crow::json::wvalue body;
    body["from"] = report.from;
    body["to"] = report.to;
    body["total_duration_sec"] = report.total_duration_sec;
    body["entries_count"] = report.entries_count;

    crow::json::wvalue::list by_task;
    by_task.reserve(report.by_task.size());
    for (const auto& item : report.by_task) {
        crow::json::wvalue obj;
        obj["task_id"] = item.task_id;
        obj["title"] = item.title;
        obj["total_duration_sec"] = item.total_duration_sec;
        by_task.push_back(std::move(obj));
    }
    body["by_task"] = std::move(by_task);

    crow::json::wvalue::list by_day;
    by_day.reserve(report.by_day.size());
    for (const auto& item : report.by_day) {
        crow::json::wvalue obj;
        obj["day"] = item.day;
        obj["total_duration_sec"] = item.total_duration_sec;
        by_day.push_back(std::move(obj));
    }
    body["by_day"] = std::move(by_day);

    return body;
}

}  // namespace

void registerReportRoutes(HttpApp& app, reports::ReportService& service) {
    CROW_ROUTE(app, "/api/v1/reports/summary")
        .methods(crow::HTTPMethod::Get)([&app, &service](const crow::request& request) {
            const auto& auth_ctx = app.get_context<auth::AuthMiddleware>(request);

            const char* from = request.url_params.get("from");
            const char* to = request.url_params.get("to");
            auto result = service.getSummary(
                auth_ctx.user_id,
                from != nullptr ? std::string(from) : "",
                to != nullptr ? std::string(to) : "");
            if (result.error.has_value()) {
                return http::error(result.error->status_code, result.error->message);
            }
            return http::json(200, toJson(*result.value));
        });
}

}  // namespace app::routes

#include "app/reports/report_service.h"

#include <exception>
#include <utility>

namespace app::reports {
namespace {

double parseDouble(const std::string& value, double fallback) {
    try {
        return std::stod(value);
    } catch (...) {
        return fallback;
    }
}

int parseInt(const std::string& value, int fallback) {
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

}  // namespace

ReportService::ReportService(std::shared_ptr<db::Postgres> db) : db_(std::move(db)) {}

SummaryReportResult ReportService::getSummary(const std::string& user_id, const std::string& from, const std::string& to) {
    if (from.empty() || to.empty()) {
        return SummaryReportResult::fail(400, "from and to are required");
    }

    try {
        SummaryReport report{
            .from = from,
            .to = to,
        };

        const std::string where =
            "te.user_id = '" + db_->escapeLiteral(user_id) +
            "' AND te.started_at >= '" + db_->escapeLiteral(from) + "'::timestamptz "
            "AND te.ended_at <= '" + db_->escapeLiteral(to) + "'::timestamptz";

        const std::string summary_sql =
            "SELECT COALESCE(SUM(te.duration_sec),0) AS total_duration_sec, COUNT(*) AS entries_count "
            "FROM time_entries te WHERE " + where;

        if (auto summary = db_->queryOne(summary_sql); summary.has_value()) {
            report.total_duration_sec = parseDouble(summary->fields["total_duration_sec"], 0.0);
            report.entries_count = parseInt(summary->fields["entries_count"], 0);
        }

        const std::string by_task_sql =
            "SELECT te.task_id, COALESCE(t.title,'') AS title, SUM(te.duration_sec) AS total_duration_sec "
            "FROM time_entries te "
            "LEFT JOIN tasks t ON t.id = te.task_id "
            "WHERE " + where + " "
            "GROUP BY te.task_id, t.title "
            "ORDER BY total_duration_sec DESC";

        auto by_task_rows = db_->queryAll(by_task_sql);
        report.by_task.reserve(by_task_rows.size());
        for (const auto& row : by_task_rows) {
            report.by_task.push_back(ReportByTask{
                .task_id = row.fields.at("task_id"),
                .title = row.fields.at("title"),
                .total_duration_sec = parseDouble(row.fields.at("total_duration_sec"), 0.0),
            });
        }

        const std::string by_day_sql =
            "SELECT to_char(te.started_at::date, 'YYYY-MM-DD') AS day, SUM(te.duration_sec) AS total_duration_sec "
            "FROM time_entries te "
            "WHERE " + where + " "
            "GROUP BY te.started_at::date "
            "ORDER BY day ASC";

        auto by_day_rows = db_->queryAll(by_day_sql);
        report.by_day.reserve(by_day_rows.size());
        for (const auto& row : by_day_rows) {
            report.by_day.push_back(ReportByDay{
                .day = row.fields.at("day"),
                .total_duration_sec = parseDouble(row.fields.at("total_duration_sec"), 0.0),
            });
        }

        return SummaryReportResult::ok(std::move(report));
    } catch (const std::exception& ex) {
        return SummaryReportResult::fail(500, ex.what());
    }
}

}  // namespace app::reports

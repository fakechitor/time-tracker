#pragma once

#include <string>
#include <vector>

#include "app/auth/models.h"

namespace app::reports {

struct ReportByTask {
    std::string task_id;
    std::string title;
    double total_duration_sec{};
};

struct ReportByDay {
    std::string day;
    double total_duration_sec{};
};

struct SummaryReport {
    std::string from;
    std::string to;
    double total_duration_sec{};
    int entries_count{};
    std::vector<ReportByTask> by_task;
    std::vector<ReportByDay> by_day;
};

using SummaryReportResult = auth::ServiceResult<SummaryReport>;

}  // namespace app::reports

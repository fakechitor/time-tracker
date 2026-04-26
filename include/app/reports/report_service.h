#pragma once

#include <memory>
#include <string>

#include "app/db/postgres.h"
#include "app/reports/models.h"

namespace app::reports {

class ReportService {
public:
    explicit ReportService(std::shared_ptr<db::Postgres> db);

    SummaryReportResult getSummary(const std::string& user_id, const std::string& from, const std::string& to);

private:
    std::shared_ptr<db::Postgres> db_;
};

}  // namespace app::reports

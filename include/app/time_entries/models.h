#pragma once

#include <string>
#include <vector>

#include "app/auth/models.h"

namespace app::time_entries {

struct TimeEntry {
    std::string id;
    std::string task_id;
    std::string user_id;
    std::string started_at;
    std::string ended_at;
    double duration_sec{};
    std::string created_at;
    std::string updated_at;
};

struct CreateTimeEntryRequest {
    std::string task_id;
    std::string started_at;
    std::string ended_at;
};

struct UpdateTimeEntryRequest {
    std::string task_id;
    std::string started_at;
    std::string ended_at;
};

struct ListTimeEntriesQuery {
    std::string task_id;
    std::string from;
    std::string to;
    std::string sort_by{"started_at"};
    std::string order{"desc"};
    int limit{50};
    int offset{0};
};

using TimeEntryResult = auth::ServiceResult<TimeEntry>;
using TimeEntriesResult = auth::ServiceResult<std::vector<TimeEntry>>;
using DeleteTimeEntryResult = auth::ServiceResult<bool>;

}  // namespace app::time_entries

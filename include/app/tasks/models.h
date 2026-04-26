#pragma once

#include <optional>
#include <string>
#include <vector>

#include "app/auth/models.h"

namespace app::tasks {

struct Task {
    std::string id;
    std::string user_id;
    std::string title;
    std::string description;
    std::string status;
    double total_time{};
    std::string created_at;
    std::string updated_at;
};

struct CreateTaskRequest {
    std::string title;
    std::string description;
    std::string status;
};

struct UpdateTaskRequest {
    std::string title;
    std::string description;
    std::string status;
};

struct ListTasksQuery {
    std::string search;
    std::string sort_by{"created_at"};
    std::string order{"desc"};
    int limit{50};
    int offset{0};
};

using TaskResult = auth::ServiceResult<Task>;
using TasksResult = auth::ServiceResult<std::vector<Task>>;
using DeleteTaskResult = auth::ServiceResult<bool>;

}  // namespace app::tasks

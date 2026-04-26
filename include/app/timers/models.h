#pragma once

#include <string>

#include "app/auth/models.h"

namespace app::timers {

struct TimerSession {
    std::string id;
    std::string task_id;
    std::string user_id;
    std::string started_at;
    std::string stopped_at;
    bool is_active{false};
    std::string created_at;
    std::string updated_at;
};

struct StartTimerRequest {
    std::string task_id;
};

struct StopTimerRequest {
    std::string task_id;
};

using TimerResult = auth::ServiceResult<TimerSession>;

}  // namespace app::timers

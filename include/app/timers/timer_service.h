#pragma once

#include <string>

#include "app/tasks/task_store.h"
#include "app/time_entries/time_entry_store.h"
#include "app/timers/timer_store.h"

namespace app::timers {

class TimerService {
public:
    TimerService(TimerStore& timer_store, tasks::TaskStore& task_store, time_entries::TimeEntryStore& time_entry_store);

    TimerResult startTimer(const std::string& user_id, const StartTimerRequest& request);
    TimerResult stopTimer(const std::string& user_id, const StopTimerRequest& request);
    TimerResult getActiveTimer(const std::string& user_id);

private:
    TimerStore& timer_store_;
    tasks::TaskStore& task_store_;
    time_entries::TimeEntryStore& time_entry_store_;
};

}  // namespace app::timers

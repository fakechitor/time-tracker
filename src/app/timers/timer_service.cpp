#include "app/timers/timer_service.h"

#include <exception>
#include <utility>

namespace app::timers {

TimerService::TimerService(
    TimerStore& timer_store,
    tasks::TaskStore& task_store,
    time_entries::TimeEntryStore& time_entry_store)
    : timer_store_(timer_store), task_store_(task_store), time_entry_store_(time_entry_store) {}

TimerResult TimerService::startTimer(const std::string& user_id, const StartTimerRequest& request) {
    if (request.task_id.empty()) {
        return TimerResult::fail(400, "task_id is required");
    }

    if (!task_store_.findById(user_id, request.task_id).has_value()) {
        return TimerResult::fail(404, "task not found");
    }

    if (timer_store_.getActiveByUser(user_id).has_value()) {
        return TimerResult::fail(409, "active timer already exists");
    }

    try {
        return TimerResult::ok(timer_store_.start(user_id, request.task_id));
    } catch (const std::exception& ex) {
        return TimerResult::fail(500, ex.what());
    }
}

TimerResult TimerService::stopTimer(const std::string& user_id, const StopTimerRequest& request) {
    auto active = timer_store_.getActiveByUser(user_id);
    if (!active.has_value()) {
        return TimerResult::fail(404, "active timer not found");
    }

    if (!request.task_id.empty() && active->task_id != request.task_id) {
        return TimerResult::fail(409, "active timer belongs to another task");
    }

    try {
        auto stopped = timer_store_.stopActive(user_id);
        if (!stopped.has_value()) {
            return TimerResult::fail(404, "active timer not found");
        }

        time_entry_store_.createRaw(
            user_id,
            stopped->task_id,
            stopped->started_at,
            stopped->stopped_at);

        return TimerResult::ok(std::move(*stopped));
    } catch (const std::exception& ex) {
        return TimerResult::fail(500, ex.what());
    }
}

TimerResult TimerService::getActiveTimer(const std::string& user_id) {
    auto active = timer_store_.getActiveByUser(user_id);
    if (!active.has_value()) {
        return TimerResult::fail(404, "active timer not found");
    }
    return TimerResult::ok(std::move(*active));
}

}  // namespace app::timers

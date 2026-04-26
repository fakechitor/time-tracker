#include "app/time_entries/time_entry_service.h"

#include <exception>
#include <utility>

namespace app::time_entries {

TimeEntryService::TimeEntryService(TimeEntryStore& store) : store_(store) {}

TimeEntryResult TimeEntryService::createTimeEntry(const std::string& user_id, const CreateTimeEntryRequest& request) {
    if (!isNonEmpty(request.task_id) || !isNonEmpty(request.started_at) || !isNonEmpty(request.ended_at)) {
        return TimeEntryResult::fail(400, "task_id, started_at and ended_at are required");
    }
    if (!store_.taskExistsForUser(user_id, request.task_id)) {
        return TimeEntryResult::fail(404, "task not found");
    }

    try {
        return TimeEntryResult::ok(store_.create(user_id, request));
    } catch (const std::exception& ex) {
        return TimeEntryResult::fail(500, ex.what());
    }
}

TimeEntryResult TimeEntryService::getTimeEntry(const std::string& user_id, const std::string& entry_id) {
    auto entry = store_.findById(user_id, entry_id);
    if (!entry.has_value()) {
        return TimeEntryResult::fail(404, "time entry not found");
    }
    return TimeEntryResult::ok(std::move(*entry));
}

TimeEntriesResult TimeEntryService::listTimeEntries(const std::string& user_id, const ListTimeEntriesQuery& query) {
    try {
        return TimeEntriesResult::ok(store_.listByUser(user_id, query));
    } catch (const std::exception& ex) {
        return TimeEntriesResult::fail(500, ex.what());
    }
}

TimeEntryResult TimeEntryService::updateTimeEntry(
    const std::string& user_id,
    const std::string& entry_id,
    const UpdateTimeEntryRequest& request) {
    if (!isNonEmpty(request.task_id) || !isNonEmpty(request.started_at) || !isNonEmpty(request.ended_at)) {
        return TimeEntryResult::fail(400, "task_id, started_at and ended_at are required");
    }
    if (!store_.taskExistsForUser(user_id, request.task_id)) {
        return TimeEntryResult::fail(404, "task not found");
    }

    try {
        auto entry = store_.update(user_id, entry_id, request);
        if (!entry.has_value()) {
            return TimeEntryResult::fail(404, "time entry not found");
        }
        return TimeEntryResult::ok(std::move(*entry));
    } catch (const std::exception& ex) {
        return TimeEntryResult::fail(500, ex.what());
    }
}

DeleteTimeEntryResult TimeEntryService::deleteTimeEntry(const std::string& user_id, const std::string& entry_id) {
    try {
        if (!store_.remove(user_id, entry_id)) {
            return DeleteTimeEntryResult::fail(404, "time entry not found");
        }
        return DeleteTimeEntryResult::ok(true);
    } catch (const std::exception& ex) {
        return DeleteTimeEntryResult::fail(500, ex.what());
    }
}

bool TimeEntryService::isNonEmpty(const std::string& value) {
    return !value.empty();
}

}  // namespace app::time_entries

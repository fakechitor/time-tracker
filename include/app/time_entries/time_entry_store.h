#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "app/db/postgres.h"
#include "app/time_entries/models.h"

namespace app::time_entries {

class TimeEntryStore {
public:
    explicit TimeEntryStore(std::shared_ptr<db::Postgres> db);

    bool taskExistsForUser(const std::string& user_id, const std::string& task_id) const;

    TimeEntry create(const std::string& user_id, const CreateTimeEntryRequest& request);
    TimeEntry createRaw(const std::string& user_id, const std::string& task_id, const std::string& started_at, const std::string& ended_at);
    std::optional<TimeEntry> findById(const std::string& user_id, const std::string& entry_id) const;
    std::vector<TimeEntry> listByUser(const std::string& user_id, const ListTimeEntriesQuery& query) const;
    std::optional<TimeEntry> update(const std::string& user_id, const std::string& entry_id, const UpdateTimeEntryRequest& request);
    bool remove(const std::string& user_id, const std::string& entry_id);

    void recalculateTaskTotalTime(const std::string& task_id);

private:
    static TimeEntry rowToTimeEntry(const db::Postgres::Row& row);
    std::string generateTimeEntryId();

    std::shared_ptr<db::Postgres> db_;
    mutable std::mutex mutex_;
    std::size_t sequence_{0};
};

}  // namespace app::time_entries

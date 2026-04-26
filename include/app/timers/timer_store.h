#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "app/db/postgres.h"
#include "app/timers/models.h"

namespace app::timers {

class TimerStore {
public:
    explicit TimerStore(std::shared_ptr<db::Postgres> db);

    std::optional<TimerSession> getActiveByUser(const std::string& user_id) const;
    TimerSession start(const std::string& user_id, const std::string& task_id);
    std::optional<TimerSession> stopActive(const std::string& user_id);

private:
    static TimerSession rowToSession(const db::Postgres::Row& row);
    std::string generateSessionId();

    std::shared_ptr<db::Postgres> db_;
    mutable std::mutex mutex_;
    std::size_t sequence_{0};
};

}  // namespace app::timers

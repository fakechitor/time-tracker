#include "app/timers/timer_store.h"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace app::timers {

TimerStore::TimerStore(std::shared_ptr<db::Postgres> db) : db_(std::move(db)) {}

std::optional<TimerSession> TimerStore::getActiveByUser(const std::string& user_id) const {
    const std::string sql =
        "SELECT id, task_id, user_id, "
        "to_char(started_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS started_at, "
        "COALESCE(to_char(stopped_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'), '') AS stopped_at, "
        "is_active, "
        "to_char(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at "
        "FROM timer_sessions WHERE user_id = '" + db_->escapeLiteral(user_id) +
        "' AND is_active = TRUE LIMIT 1";

    auto row = db_->queryOne(sql);
    if (!row.has_value()) {
        return std::nullopt;
    }
    return rowToSession(*row);
}

TimerSession TimerStore::start(const std::string& user_id, const std::string& task_id) {
    const std::string session_id = generateSessionId();
    const std::string sql =
        "INSERT INTO timer_sessions (id, task_id, user_id, started_at, is_active) VALUES ('" +
        db_->escapeLiteral(session_id) + "', '" +
        db_->escapeLiteral(task_id) + "', '" +
        db_->escapeLiteral(user_id) + "', NOW(), TRUE) "
        "RETURNING id, task_id, user_id, "
        "to_char(started_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS started_at, "
        "COALESCE(to_char(stopped_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'), '') AS stopped_at, "
        "is_active, "
        "to_char(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at";

    std::string error;
    auto row = db_->queryOne(sql, &error);
    if (!row.has_value()) {
        throw std::runtime_error("Failed to start timer: " + error);
    }
    return rowToSession(*row);
}

std::optional<TimerSession> TimerStore::stopActive(const std::string& user_id) {
    const std::string sql =
        "UPDATE timer_sessions SET stopped_at = NOW(), is_active = FALSE, updated_at = NOW() "
        "WHERE id = (SELECT id FROM timer_sessions WHERE user_id = '" + db_->escapeLiteral(user_id) +
        "' AND is_active = TRUE ORDER BY started_at DESC LIMIT 1) "
        "RETURNING id, task_id, user_id, "
        "to_char(started_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS started_at, "
        "COALESCE(to_char(stopped_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'), '') AS stopped_at, "
        "is_active, "
        "to_char(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at";

    auto row = db_->queryOne(sql);
    if (!row.has_value()) {
        return std::nullopt;
    }
    return rowToSession(*row);
}

TimerSession TimerStore::rowToSession(const db::Postgres::Row& row) {
    const auto& f = row.fields;
    TimerSession session{
        .id = f.at("id"),
        .task_id = f.at("task_id"),
        .user_id = f.at("user_id"),
        .started_at = f.at("started_at"),
        .stopped_at = f.at("stopped_at"),
        .is_active = (f.at("is_active") == "t" || f.at("is_active") == "true" || f.at("is_active") == "1"),
        .created_at = f.at("created_at"),
        .updated_at = f.at("updated_at"),
    };
    return session;
}

std::string TimerStore::generateSessionId() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++sequence_;
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    return "ts-" + std::to_string(now) + "-" + std::to_string(sequence_);
}

}  // namespace app::timers

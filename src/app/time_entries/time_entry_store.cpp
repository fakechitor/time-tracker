#include "app/time_entries/time_entry_store.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace app::time_entries {
namespace {

double parseDouble(const std::string& value, double fallback) {
    try {
        return std::stod(value);
    } catch (...) {
        return fallback;
    }
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

}  // namespace

TimeEntryStore::TimeEntryStore(std::shared_ptr<db::Postgres> db) : db_(std::move(db)) {}

bool TimeEntryStore::taskExistsForUser(const std::string& user_id, const std::string& task_id) const {
    const std::string sql =
        "SELECT id FROM tasks WHERE id = '" + db_->escapeLiteral(task_id) +
        "' AND user_id = '" + db_->escapeLiteral(user_id) + "' LIMIT 1";
    return db_->queryOne(sql).has_value();
}

TimeEntry TimeEntryStore::create(const std::string& user_id, const CreateTimeEntryRequest& request) {
    return createRaw(user_id, request.task_id, request.started_at, request.ended_at);
}

TimeEntry TimeEntryStore::createRaw(
    const std::string& user_id,
    const std::string& task_id,
    const std::string& started_at,
    const std::string& ended_at) {
    const std::string id = generateTimeEntryId();
    const std::string sql =
        "INSERT INTO time_entries (id, task_id, user_id, started_at, ended_at, duration_sec) VALUES ('" +
        db_->escapeLiteral(id) + "', '" +
        db_->escapeLiteral(task_id) + "', '" +
        db_->escapeLiteral(user_id) + "', '" +
        db_->escapeLiteral(started_at) + "'::timestamptz, '" +
        db_->escapeLiteral(ended_at) + "'::timestamptz, "
        "EXTRACT(EPOCH FROM ('" + db_->escapeLiteral(ended_at) + "'::timestamptz - '" +
        db_->escapeLiteral(started_at) + "'::timestamptz))"
        ") RETURNING id, task_id, user_id, "
        "to_char(started_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS started_at, "
        "to_char(ended_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS ended_at, "
        "duration_sec, "
        "to_char(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at";

    std::string error;
    auto row = db_->queryOne(sql, &error);
    if (!row.has_value()) {
        throw std::runtime_error("Failed to create time entry: " + error);
    }

    recalculateTaskTotalTime(task_id);
    return rowToTimeEntry(*row);
}

std::optional<TimeEntry> TimeEntryStore::findById(const std::string& user_id, const std::string& entry_id) const {
    const std::string sql =
        "SELECT id, task_id, user_id, "
        "to_char(started_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS started_at, "
        "to_char(ended_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS ended_at, "
        "duration_sec, "
        "to_char(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at "
        "FROM time_entries WHERE id = '" + db_->escapeLiteral(entry_id) +
        "' AND user_id = '" + db_->escapeLiteral(user_id) + "' LIMIT 1";

    auto row = db_->queryOne(sql);
    if (!row.has_value()) {
        return std::nullopt;
    }
    return rowToTimeEntry(*row);
}

std::vector<TimeEntry> TimeEntryStore::listByUser(const std::string& user_id, const ListTimeEntriesQuery& query) const {
    std::string sort_by = toLower(query.sort_by);
    if (sort_by != "started_at" && sort_by != "ended_at" && sort_by != "created_at" && sort_by != "duration_sec") {
        sort_by = "started_at";
    }

    std::string order = toLower(query.order);
    if (order != "asc" && order != "desc") {
        order = "desc";
    }

    int limit = query.limit;
    if (limit <= 0) {
        limit = 50;
    }
    if (limit > 200) {
        limit = 200;
    }

    int offset = query.offset;
    if (offset < 0) {
        offset = 0;
    }

    std::string where = "user_id = '" + db_->escapeLiteral(user_id) + "'";
    if (!query.task_id.empty()) {
        where += " AND task_id = '" + db_->escapeLiteral(query.task_id) + "'";
    }
    if (!query.from.empty()) {
        where += " AND started_at >= '" + db_->escapeLiteral(query.from) + "'::timestamptz";
    }
    if (!query.to.empty()) {
        where += " AND ended_at <= '" + db_->escapeLiteral(query.to) + "'::timestamptz";
    }

    const std::string sql =
        "SELECT id, task_id, user_id, "
        "to_char(started_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS started_at, "
        "to_char(ended_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS ended_at, "
        "duration_sec, "
        "to_char(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at "
        "FROM time_entries WHERE " + where + " ORDER BY " + sort_by + " " + order +
        " LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    std::vector<db::Postgres::Row> rows = db_->queryAll(sql);
    std::vector<TimeEntry> entries;
    entries.reserve(rows.size());
    for (const auto& row : rows) {
        entries.push_back(rowToTimeEntry(row));
    }
    return entries;
}

std::optional<TimeEntry> TimeEntryStore::update(
    const std::string& user_id,
    const std::string& entry_id,
    const UpdateTimeEntryRequest& request) {
    auto current = findById(user_id, entry_id);
    if (!current.has_value()) {
        return std::nullopt;
    }

    const std::string sql =
        "UPDATE time_entries SET task_id = '" + db_->escapeLiteral(request.task_id) +
        "', started_at = '" + db_->escapeLiteral(request.started_at) + "'::timestamptz, "
        "ended_at = '" + db_->escapeLiteral(request.ended_at) + "'::timestamptz, "
        "duration_sec = EXTRACT(EPOCH FROM ('" + db_->escapeLiteral(request.ended_at) + "'::timestamptz - '" +
        db_->escapeLiteral(request.started_at) + "'::timestamptz)), "
        "updated_at = NOW() "
        "WHERE id = '" + db_->escapeLiteral(entry_id) + "' "
        "AND user_id = '" + db_->escapeLiteral(user_id) + "' "
        "RETURNING id, task_id, user_id, "
        "to_char(started_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS started_at, "
        "to_char(ended_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS ended_at, "
        "duration_sec, "
        "to_char(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at";

    std::string error;
    auto row = db_->queryOne(sql, &error);
    if (!row.has_value()) {
        return std::nullopt;
    }

    recalculateTaskTotalTime(current->task_id);
    if (current->task_id != request.task_id) {
        recalculateTaskTotalTime(request.task_id);
    }
    return rowToTimeEntry(*row);
}

bool TimeEntryStore::remove(const std::string& user_id, const std::string& entry_id) {
    auto current = findById(user_id, entry_id);
    if (!current.has_value()) {
        return false;
    }

    const std::string sql =
        "DELETE FROM time_entries WHERE id = '" + db_->escapeLiteral(entry_id) +
        "' AND user_id = '" + db_->escapeLiteral(user_id) + "' RETURNING id";
    const bool deleted = db_->queryOne(sql).has_value();
    if (deleted) {
        recalculateTaskTotalTime(current->task_id);
    }
    return deleted;
}

void TimeEntryStore::recalculateTaskTotalTime(const std::string& task_id) {
    const std::string sql =
        "UPDATE tasks SET total_time = COALESCE((SELECT SUM(duration_sec) FROM time_entries WHERE task_id = '" +
        db_->escapeLiteral(task_id) + "'), 0), updated_at = NOW() WHERE id = '" + db_->escapeLiteral(task_id) + "'";
    std::string error;
    if (!db_->execute(sql, &error)) {
        throw std::runtime_error("Failed to recalculate task total_time: " + error);
    }
}

TimeEntry TimeEntryStore::rowToTimeEntry(const db::Postgres::Row& row) {
    const auto& f = row.fields;
    TimeEntry entry{
        .id = f.at("id"),
        .task_id = f.at("task_id"),
        .user_id = f.at("user_id"),
        .started_at = f.at("started_at"),
        .ended_at = f.at("ended_at"),
        .duration_sec = parseDouble(f.at("duration_sec"), 0.0),
        .created_at = f.at("created_at"),
        .updated_at = f.at("updated_at"),
    };
    return entry;
}

std::string TimeEntryStore::generateTimeEntryId() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++sequence_;
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    return "te-" + std::to_string(now) + "-" + std::to_string(sequence_);
}

}  // namespace app::time_entries

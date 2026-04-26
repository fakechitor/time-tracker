#include "app/tasks/task_store.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace app::tasks {
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

TaskStore::TaskStore(std::shared_ptr<db::Postgres> db) : db_(std::move(db)) {}

Task TaskStore::create(const std::string& user_id, const CreateTaskRequest& request) {
    const std::string id = generateTaskId();
    const std::string sql =
        "INSERT INTO tasks (id, user_id, title, description, status, total_time) VALUES ('" +
        db_->escapeLiteral(id) + "', '" +
        db_->escapeLiteral(user_id) + "', '" +
        db_->escapeLiteral(request.title) + "', '" +
        db_->escapeLiteral(request.description) + "', '" +
        db_->escapeLiteral(request.status) + "', 0) "
        "RETURNING id, user_id, title, description, status, total_time, "
        "to_char(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at";

    std::string error;
    auto row = db_->queryOne(sql, &error);
    if (!row.has_value()) {
        throw std::runtime_error("Failed to create task: " + error);
    }
    return rowToTask(*row);
}

std::optional<Task> TaskStore::findById(const std::string& user_id, const std::string& task_id) const {
    const std::string sql =
        "SELECT id, user_id, title, description, status, total_time, "
        "to_char(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at "
        "FROM tasks WHERE id = '" + db_->escapeLiteral(task_id) + "' "
        "AND user_id = '" + db_->escapeLiteral(user_id) + "' LIMIT 1";

    auto row = db_->queryOne(sql);
    if (!row.has_value()) {
        return std::nullopt;
    }
    return rowToTask(*row);
}

std::vector<Task> TaskStore::listByUser(const std::string& user_id, const ListTasksQuery& query) const {
    std::string sort_by = toLower(query.sort_by);
    if (sort_by != "created_at" && sort_by != "updated_at" && sort_by != "title" && sort_by != "status") {
        sort_by = "created_at";
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

    std::string where =
        "user_id = '" + db_->escapeLiteral(user_id) + "'";
    if (!query.search.empty()) {
        const std::string pattern = "%" + escapeForLike(query.search) + "%";
        where += " AND (title ILIKE '" + db_->escapeLiteral(pattern) + "' ESCAPE '\\' "
                 "OR description ILIKE '" + db_->escapeLiteral(pattern) + "' ESCAPE '\\')";
    }

    const std::string sql =
        "SELECT id, user_id, title, description, status, total_time, "
        "to_char(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at "
        "FROM tasks WHERE " + where + " ORDER BY " + sort_by + " " + order +
        " LIMIT " + std::to_string(limit) +
        " OFFSET " + std::to_string(offset);

    std::vector<db::Postgres::Row> rows = db_->queryAll(sql);
    std::vector<Task> tasks;
    tasks.reserve(rows.size());
    for (const auto& row : rows) {
        tasks.push_back(rowToTask(row));
    }
    return tasks;
}

std::optional<Task> TaskStore::update(
    const std::string& user_id,
    const std::string& task_id,
    const UpdateTaskRequest& request) {
    const std::string sql =
        "UPDATE tasks SET title = '" + db_->escapeLiteral(request.title) +
        "', description = '" + db_->escapeLiteral(request.description) +
        "', status = '" + db_->escapeLiteral(request.status) +
        "', updated_at = NOW() WHERE id = '" + db_->escapeLiteral(task_id) +
        "' AND user_id = '" + db_->escapeLiteral(user_id) + "' "
        "RETURNING id, user_id, title, description, status, total_time, "
        "to_char(created_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at, "
        "to_char(updated_at, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS updated_at";

    auto row = db_->queryOne(sql);
    if (!row.has_value()) {
        return std::nullopt;
    }
    return rowToTask(*row);
}

bool TaskStore::remove(const std::string& user_id, const std::string& task_id) {
    const std::string sql =
        "DELETE FROM tasks WHERE id = '" + db_->escapeLiteral(task_id) +
        "' AND user_id = '" + db_->escapeLiteral(user_id) +
        "' RETURNING id";
    return db_->queryOne(sql).has_value();
}

Task TaskStore::rowToTask(const db::Postgres::Row& row) {
    const auto& f = row.fields;
    Task task{
        .id = f.at("id"),
        .user_id = f.at("user_id"),
        .title = f.at("title"),
        .description = f.at("description"),
        .status = f.at("status"),
        .total_time = parseDouble(f.at("total_time"), 0.0),
        .created_at = f.at("created_at"),
        .updated_at = f.at("updated_at"),
    };
    return task;
}

std::string TaskStore::escapeForLike(const std::string& value) {
    std::string result;
    result.reserve(value.size() * 2);
    for (char ch : value) {
        if (ch == '%' || ch == '_' || ch == '\\') {
            result.push_back('\\');
        }
        result.push_back(ch);
    }
    return result;
}

std::string TaskStore::generateTaskId() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++sequence_;
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    return "task-" + std::to_string(now) + "-" + std::to_string(sequence_);
}

}  // namespace app::tasks

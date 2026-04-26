#pragma once

#include <cstddef>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/db/postgres.h"
#include "app/tasks/models.h"

namespace app::tasks {

class TaskStore {
public:
    explicit TaskStore(std::shared_ptr<db::Postgres> db);

    Task create(const std::string& user_id, const CreateTaskRequest& request);
    std::optional<Task> findById(const std::string& user_id, const std::string& task_id) const;
    std::vector<Task> listByUser(const std::string& user_id, const ListTasksQuery& query) const;
    std::optional<Task> update(const std::string& user_id, const std::string& task_id, const UpdateTaskRequest& request);
    bool remove(const std::string& user_id, const std::string& task_id);

private:
    static Task rowToTask(const db::Postgres::Row& row);
    static std::string escapeForLike(const std::string& value);
    std::string generateTaskId();

    std::shared_ptr<db::Postgres> db_;
    mutable std::mutex mutex_;
    std::size_t sequence_{0};
};

}  // namespace app::tasks

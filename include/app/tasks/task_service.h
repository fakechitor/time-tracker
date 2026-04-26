#pragma once

#include <string>

#include "app/tasks/task_store.h"

namespace app::tasks {

class TaskService {
public:
    explicit TaskService(TaskStore& task_store);

    TaskResult createTask(const std::string& user_id, const CreateTaskRequest& request);
    TaskResult getTask(const std::string& user_id, const std::string& task_id);
    TasksResult listTasks(const std::string& user_id, const ListTasksQuery& query);
    TaskResult updateTask(const std::string& user_id, const std::string& task_id, const UpdateTaskRequest& request);
    DeleteTaskResult deleteTask(const std::string& user_id, const std::string& task_id);

private:
    static bool isValidStatus(const std::string& status);
    static std::string normalizeStatus(std::string status);

    TaskStore& task_store_;
};

}  // namespace app::tasks

#include "app/tasks/task_service.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <utility>

namespace app::tasks {

TaskService::TaskService(TaskStore& task_store) : task_store_(task_store) {}

TaskResult TaskService::createTask(const std::string& user_id, const CreateTaskRequest& request) {
    if (request.title.empty()) {
        return TaskResult::fail(400, "title is required");
    }

    const std::string status = normalizeStatus(request.status.empty() ? "active" : request.status);
    if (!isValidStatus(status)) {
        return TaskResult::fail(400, "status must be one of: active, completed, archived");
    }

    try {
        Task created = task_store_.create(user_id, CreateTaskRequest{
                                                      .title = request.title,
                                                      .description = request.description,
                                                      .status = status,
                                                  });
        return TaskResult::ok(std::move(created));
    } catch (const std::exception& ex) {
        return TaskResult::fail(500, ex.what());
    }
}

TaskResult TaskService::getTask(const std::string& user_id, const std::string& task_id) {
    auto task = task_store_.findById(user_id, task_id);
    if (!task.has_value()) {
        return TaskResult::fail(404, "task not found");
    }
    return TaskResult::ok(std::move(*task));
}

TasksResult TaskService::listTasks(const std::string& user_id, const ListTasksQuery& query) {
    try {
        return TasksResult::ok(task_store_.listByUser(user_id, query));
    } catch (const std::exception& ex) {
        return TasksResult::fail(500, ex.what());
    }
}

TaskResult TaskService::updateTask(
    const std::string& user_id,
    const std::string& task_id,
    const UpdateTaskRequest& request) {
    if (request.title.empty()) {
        return TaskResult::fail(400, "title is required");
    }

    const std::string status = normalizeStatus(request.status);
    if (!isValidStatus(status)) {
        return TaskResult::fail(400, "status must be one of: active, completed, archived");
    }

    auto task = task_store_.update(user_id, task_id, UpdateTaskRequest{
                                                    .title = request.title,
                                                    .description = request.description,
                                                    .status = status,
                                                });
    if (!task.has_value()) {
        return TaskResult::fail(404, "task not found");
    }
    return TaskResult::ok(std::move(*task));
}

DeleteTaskResult TaskService::deleteTask(const std::string& user_id, const std::string& task_id) {
    if (!task_store_.remove(user_id, task_id)) {
        return DeleteTaskResult::fail(404, "task not found");
    }
    return DeleteTaskResult::ok(true);
}

bool TaskService::isValidStatus(const std::string& status) {
    return status == "active" || status == "completed" || status == "archived";
}

std::string TaskService::normalizeStatus(std::string status) {
    std::transform(status.begin(), status.end(), status.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return status;
}

}  // namespace app::tasks

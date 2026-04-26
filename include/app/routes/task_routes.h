#pragma once

#include "app/app_types.h"
#include "app/tasks/task_service.h"

namespace app::routes {

void registerTaskRoutes(HttpApp& app, tasks::TaskService& task_service);

}  // namespace app::routes

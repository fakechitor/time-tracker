#pragma once

#include "app/app_types.h"
#include "app/timers/timer_service.h"

namespace app::routes {

void registerTimerRoutes(HttpApp& app, timers::TimerService& service);

}  // namespace app::routes

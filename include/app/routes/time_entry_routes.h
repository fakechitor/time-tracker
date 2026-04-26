#pragma once

#include "app/app_types.h"
#include "app/time_entries/time_entry_service.h"

namespace app::routes {

void registerTimeEntryRoutes(HttpApp& app, time_entries::TimeEntryService& service);

}  // namespace app::routes

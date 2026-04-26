#pragma once

#include "app/app_types.h"
#include "app/reports/report_service.h"

namespace app::routes {

void registerReportRoutes(HttpApp& app, reports::ReportService& service);

}  // namespace app::routes

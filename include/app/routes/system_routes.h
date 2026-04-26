#pragma once

#include "app/app_types.h"
#include "app/auth/auth_service.h"
#include "app/db/postgres.h"

namespace app::routes {

void registerSystemRoutes(HttpApp& app, auth::AuthService& auth_service, db::Postgres& postgres);

}  // namespace app::routes

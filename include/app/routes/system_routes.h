#pragma once

#include "app/app_types.h"
#include "app/auth/auth_service.h"

namespace app::routes {

void registerSystemRoutes(HttpApp& app, auth::AuthService& auth_service);

}  // namespace app::routes


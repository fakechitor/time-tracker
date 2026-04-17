#pragma once

#include "app/app_types.h"
#include "app/auth/auth_service.h"

namespace app::routes {

void registerAuthRoutes(HttpApp& app, auth::AuthService& auth_service);

}  // namespace app::routes


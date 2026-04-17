#pragma once

#include <crow.h>

#include "app/auth/auth_middleware.h"

namespace app {

using HttpApp = crow::App<auth::AuthMiddleware>;

}  // namespace app


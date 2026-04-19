#pragma once

#include <crow.h>
#include <crow/middlewares/cors.h>

#include "app/auth/auth_middleware.h"

namespace app {

using HttpApp = crow::App<crow::CORSHandler, auth::AuthMiddleware>;

}  // namespace app

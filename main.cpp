#include <cstdlib>
#include <memory>
#include <string>

#include "app/app_types.h"
#include "app/auth/auth_middleware.h"
#include "app/auth/auth_service.h"
#include "app/auth/jwt_service.h"
#include "app/auth/user_store.h"
#include "app/routes/auth_routes.h"
#include "app/routes/system_routes.h"

namespace {

std::string getEnvOrDefault(const char* key, std::string fallback) {
    const char* value = std::getenv(key);
    if (value == nullptr || std::string(value).empty()) {
        return fallback;
    }
    return value;
}

int getPort() {
    const std::string raw = getEnvOrDefault("APP_PORT", "18080");
    try {
        return std::stoi(raw);
    } catch (...) {
        return 18080;
    }
}

}  // namespace

int main() {
    auto user_store = std::make_shared<app::auth::UserStore>();

    auto jwt_service = std::make_shared<app::auth::JwtService>(
        getEnvOrDefault("JWT_SECRET", "dev-secret-change-me"),
        "time-tracker-app",
        "time-tracker-client",
        15 * 60,
        7 * 24 * 60 * 60);

    app::auth::AuthService auth_service(*user_store, *jwt_service);
    app::auth::AuthMiddleware::configure(jwt_service);

    app::HttpApp app;
    app::routes::registerSystemRoutes(app, auth_service);
    app::routes::registerAuthRoutes(app, auth_service);

    app.port(getPort()).multithreaded().run();
    return 0;
}


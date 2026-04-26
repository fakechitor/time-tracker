#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "app/app_types.h"
#include "app/auth/auth_middleware.h"
#include "app/auth/auth_service.h"
#include "app/auth/jwt_service.h"
#include "app/auth/user_store.h"
#include "app/db/postgres.h"
#include "app/routes/auth_routes.h"
#include "app/routes/system_routes.h"

namespace {

std::string trim(std::string s) {
    const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

bool loadDotEnvFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        const auto pos = line.find('=');
        if (pos == std::string::npos || pos == 0) {
            continue;
        }

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));
        if (key.empty()) {
            continue;
        }

        if (value.size() >= 2 &&
            ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

#if defined(_WIN32)
        _putenv_s(key.c_str(), value.c_str());
#else
        setenv(key.c_str(), value.c_str(), 1);
#endif
    }
    return true;
}

void loadDotEnv() {
    std::filesystem::path current = std::filesystem::current_path();
    while (true) {
        const auto candidate = current / ".env";
        if (loadDotEnvFile(candidate)) {
            return;
        }
        const auto parent = current.parent_path();
        if (parent == current || parent.empty()) {
            break;
        }
        current = parent;
    }
}

std::string getEnvOrDefault(const char* key, std::string fallback) {
    const char* value = std::getenv(key);
    if (value == nullptr || std::string(value).empty()) {
        return fallback;
    }
    return value;
}

int getIntEnvOrDefault(const char* key, int fallback) {
    const std::string raw = getEnvOrDefault(key, std::to_string(fallback));
    try {
        return std::stoi(raw);
    } catch (...) {
        return fallback;
    }
}

int getPort() {
    return getIntEnvOrDefault("APP_PORT", 18080);
}

}  // namespace

int main() {
    loadDotEnv();

    app::db::PostgresConfig db_config{
        .host = getEnvOrDefault("DB_HOST", "127.0.0.1"),
        .port = getIntEnvOrDefault("DB_PORT", 5432),
        .dbname = getEnvOrDefault("DB_NAME", "time-tracker"),
        .user = getEnvOrDefault("DB_USER", "user"),
        .password = getEnvOrDefault("DB_PASSWORD", "password"),
        .sslmode = getEnvOrDefault("DB_SSLMODE", "disable"),
    };
    auto postgres = std::make_shared<app::db::Postgres>(db_config);

    std::string db_error;
    if (!postgres->ping(&db_error)) {
        std::cerr << "Failed to connect to PostgreSQL: " << db_error << '\n';
        return 1;
    }
    postgres->ensureSchema();

    auto user_store = std::make_shared<app::auth::UserStore>(postgres);

    auto jwt_service = std::make_shared<app::auth::JwtService>(
        getEnvOrDefault("JWT_SECRET", "dev-secret-change-me"),
        "time-tracker-app",
        "time-tracker-client",
        15 * 60,
        7 * 24 * 60 * 60);

    app::auth::AuthService auth_service(*user_store, *jwt_service);
    app::auth::AuthMiddleware::configure(jwt_service);

    app::HttpApp app;
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin("*")
        .headers("*")
        .methods(crow::HTTPMethod::Get,
                 crow::HTTPMethod::Post,
                 crow::HTTPMethod::Put,
                 crow::HTTPMethod::Delete,
                 crow::HTTPMethod::Patch,
                 crow::HTTPMethod::Head,
                 crow::HTTPMethod::Options);

    app::routes::registerSystemRoutes(app, auth_service, *postgres);
    app::routes::registerAuthRoutes(app, auth_service);

    app.port(getPort()).multithreaded().run();
    return 0;
}

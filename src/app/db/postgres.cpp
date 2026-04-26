#include "app/db/postgres.h"

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace app::db {
namespace {

struct PGconn;
struct PGresult;

enum ConnStatusType {
    CONNECTION_OK = 0,
};

enum ExecStatusType {
    PGRES_EMPTY_QUERY = 0,
    PGRES_COMMAND_OK = 1,
    PGRES_TUPLES_OK = 2,
};

using PQconnectdbFn = PGconn* (*)(const char*);
using PQstatusFn = ConnStatusType (*)(const PGconn*);
using PQerrorMessageFn = char* (*)(const PGconn*);
using PQfinishFn = void (*)(PGconn*);
using PQexecFn = PGresult* (*)(PGconn*, const char*);
using PQresultStatusFn = ExecStatusType (*)(const PGresult*);
using PQntuplesFn = int (*)(const PGresult*);
using PQnfieldsFn = int (*)(const PGresult*);
using PQfnameFn = char* (*)(const PGresult*, int);
using PQgetvalueFn = char* (*)(const PGresult*, int, int);
using PQclearFn = void (*)(PGresult*);
using PQescapeStringConnFn = size_t (*)(PGconn*, char*, const char*, size_t, int*);

struct LibpqApi {
    PQconnectdbFn PQconnectdb{nullptr};
    PQstatusFn PQstatus{nullptr};
    PQerrorMessageFn PQerrorMessage{nullptr};
    PQfinishFn PQfinish{nullptr};
    PQexecFn PQexec{nullptr};
    PQresultStatusFn PQresultStatus{nullptr};
    PQntuplesFn PQntuples{nullptr};
    PQnfieldsFn PQnfields{nullptr};
    PQfnameFn PQfname{nullptr};
    PQgetvalueFn PQgetvalue{nullptr};
    PQclearFn PQclear{nullptr};
    PQescapeStringConnFn PQescapeStringConn{nullptr};
#if defined(_WIN32)
    HMODULE module{nullptr};
#else
    void* module{nullptr};
#endif
    bool loaded{false};
};

LibpqApi& libpq() {
    static LibpqApi api;
    return api;
}

void* loadSymbol(
#if defined(_WIN32)
    HMODULE module,
#else
    void* module,
#endif
    const char* name) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(module, name));
#else
    return dlsym(module, name);
#endif
}

void ensureLibpqLoaded() {
    static std::once_flag once;
    static std::string load_error;
    std::call_once(once, [] {
        auto& api = libpq();
#if defined(_WIN32)
        auto winErrorMessage = [](DWORD code) -> std::string {
            if (code == 0) {
                return "unknown error";
            }
            LPSTR message_buffer = nullptr;
            const DWORD size = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                code,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPSTR>(&message_buffer),
                0,
                nullptr);
            if (size == 0 || message_buffer == nullptr) {
                return "error code " + std::to_string(code);
            }
            std::string message(message_buffer, size);
            LocalFree(message_buffer);
            message.erase(std::remove(message.begin(), message.end(), '\r'), message.end());
            message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
            return message;
        };

        std::vector<std::string> candidates;
        if (const char* env_dll = std::getenv("DB_LIBPQ_DLL"); env_dll != nullptr && *env_dll != '\0') {
            candidates.emplace_back(env_dll);
        }
        candidates.emplace_back("libpq.dll");

        const char* program_files = std::getenv("ProgramFiles");
        const std::string program_files_root = program_files != nullptr ? std::string(program_files) : "C:\\Program Files";
        for (int version = 20; version >= 10; --version) {
            candidates.push_back(program_files_root + "\\PostgreSQL\\" + std::to_string(version) + "\\bin\\libpq.dll");
        }

        std::string last_error;
        for (const auto& candidate : candidates) {
            if (candidate.find(':') != std::string::npos || candidate.find('\\') != std::string::npos ||
                candidate.find('/') != std::string::npos) {
                api.module = LoadLibraryExA(candidate.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
            } else {
                api.module = LoadLibraryA(candidate.c_str());
            }
            if (api.module != nullptr) {
                break;
            }
            last_error = candidate + ": " + winErrorMessage(GetLastError());
        }

        if (api.module == nullptr) {
            load_error = "libpq.dll not found or failed to load. "
                         "Set DB_LIBPQ_DLL to full dll path or add PostgreSQL\\bin to PATH. Last attempt: " +
                         last_error;
            return;
        }
#else
        api.module = dlopen("libpq.so.5", RTLD_LAZY);
        if (api.module == nullptr) {
            api.module = dlopen("libpq.so", RTLD_LAZY);
        }
        if (api.module == nullptr) {
            api.module = dlopen("libpq.dylib", RTLD_LAZY);
        }
        if (api.module == nullptr) {
            load_error = "libpq shared library not found.";
            return;
        }
#endif

        api.PQconnectdb = reinterpret_cast<PQconnectdbFn>(loadSymbol(api.module, "PQconnectdb"));
        api.PQstatus = reinterpret_cast<PQstatusFn>(loadSymbol(api.module, "PQstatus"));
        api.PQerrorMessage = reinterpret_cast<PQerrorMessageFn>(loadSymbol(api.module, "PQerrorMessage"));
        api.PQfinish = reinterpret_cast<PQfinishFn>(loadSymbol(api.module, "PQfinish"));
        api.PQexec = reinterpret_cast<PQexecFn>(loadSymbol(api.module, "PQexec"));
        api.PQresultStatus = reinterpret_cast<PQresultStatusFn>(loadSymbol(api.module, "PQresultStatus"));
        api.PQntuples = reinterpret_cast<PQntuplesFn>(loadSymbol(api.module, "PQntuples"));
        api.PQnfields = reinterpret_cast<PQnfieldsFn>(loadSymbol(api.module, "PQnfields"));
        api.PQfname = reinterpret_cast<PQfnameFn>(loadSymbol(api.module, "PQfname"));
        api.PQgetvalue = reinterpret_cast<PQgetvalueFn>(loadSymbol(api.module, "PQgetvalue"));
        api.PQclear = reinterpret_cast<PQclearFn>(loadSymbol(api.module, "PQclear"));
        api.PQescapeStringConn =
            reinterpret_cast<PQescapeStringConnFn>(loadSymbol(api.module, "PQescapeStringConn"));

        if (!api.PQconnectdb || !api.PQstatus || !api.PQerrorMessage || !api.PQfinish || !api.PQexec ||
            !api.PQresultStatus || !api.PQntuples || !api.PQnfields || !api.PQfname || !api.PQgetvalue ||
            !api.PQclear || !api.PQescapeStringConn) {
            load_error = "Failed to load required symbols from libpq.";
            return;
        }

        api.loaded = true;
    });

    if (!libpq().loaded) {
        throw std::runtime_error(load_error.empty() ? "libpq not loaded." : load_error);
    }
}

struct ConnectionHandle {
    explicit ConnectionHandle(const std::string& conn_str) {
        auto& api = libpq();
        conn = api.PQconnectdb(conn_str.c_str());
    }

    ~ConnectionHandle() {
        if (conn != nullptr) {
            libpq().PQfinish(conn);
        }
    }

    PGconn* conn{nullptr};
};

std::string connError(PGconn* conn) {
    const auto* message = libpq().PQerrorMessage(conn);
    if (message == nullptr) {
        return "unknown libpq error";
    }
    return std::string(message);
}

}  // namespace

std::string PostgresConfig::connectionString() const {
    return "host=" + host +
           " port=" + std::to_string(port) +
           " dbname=" + dbname +
           " user=" + user +
           " password=" + password +
           " sslmode=" + sslmode;
}

Postgres::Postgres(PostgresConfig config) : connection_string_(config.connectionString()) {}

Postgres::~Postgres() = default;

void Postgres::ensureLoaded() const {
    ensureLibpqLoaded();
}

void Postgres::ensureSchema() const {
    std::string error;
    if (!execute(
            R"SQL(
        CREATE TABLE IF NOT EXISTS users (
            id TEXT PRIMARY KEY,
            username TEXT NOT NULL,
            email TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
        )
    )SQL",
            &error)) {
        throw std::runtime_error("Failed to ensure schema: " + error);
    }

    if (!execute(
            R"SQL(
        CREATE TABLE IF NOT EXISTS tasks (
            id TEXT PRIMARY KEY,
            user_id TEXT NOT NULL,
            title TEXT NOT NULL,
            description TEXT NOT NULL DEFAULT '',
            status TEXT NOT NULL DEFAULT 'active',
            total_time DOUBLE PRECISION NOT NULL DEFAULT 0,
            created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
            updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
            CONSTRAINT fk_tasks_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
        )
    )SQL",
            &error)) {
        throw std::runtime_error("Failed to ensure schema: " + error);
    }

    if (!execute(
            R"SQL(
        CREATE INDEX IF NOT EXISTS idx_tasks_user_created_at ON tasks (user_id, created_at DESC)
    )SQL",
            &error)) {
        throw std::runtime_error("Failed to ensure tasks index schema: " + error);
    }

    if (!execute(
            R"SQL(
        CREATE TABLE IF NOT EXISTS time_entries (
            id TEXT PRIMARY KEY,
            task_id TEXT NOT NULL,
            user_id TEXT NOT NULL,
            started_at TIMESTAMPTZ NOT NULL,
            ended_at TIMESTAMPTZ NOT NULL,
            duration_sec DOUBLE PRECISION NOT NULL,
            created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
            updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
            CONSTRAINT fk_time_entries_task FOREIGN KEY (task_id) REFERENCES tasks (id) ON DELETE CASCADE,
            CONSTRAINT fk_time_entries_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE,
            CONSTRAINT chk_time_entries_time_range CHECK (ended_at >= started_at)
        )
    )SQL",
            &error)) {
        throw std::runtime_error("Failed to ensure time_entries schema: " + error);
    }

    if (!execute(
            R"SQL(
        CREATE INDEX IF NOT EXISTS idx_time_entries_user_started_at ON time_entries (user_id, started_at DESC)
    )SQL",
            &error)) {
        throw std::runtime_error("Failed to ensure time_entries user index schema: " + error);
    }

    if (!execute(
            R"SQL(
        CREATE INDEX IF NOT EXISTS idx_time_entries_task_started_at ON time_entries (task_id, started_at DESC)
    )SQL",
            &error)) {
        throw std::runtime_error("Failed to ensure time_entries task index schema: " + error);
    }

    if (!execute(
            R"SQL(
        CREATE TABLE IF NOT EXISTS timer_sessions (
            id TEXT PRIMARY KEY,
            task_id TEXT NOT NULL,
            user_id TEXT NOT NULL,
            started_at TIMESTAMPTZ NOT NULL,
            stopped_at TIMESTAMPTZ,
            is_active BOOLEAN NOT NULL DEFAULT TRUE,
            created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
            updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
            CONSTRAINT fk_timer_sessions_task FOREIGN KEY (task_id) REFERENCES tasks (id) ON DELETE CASCADE,
            CONSTRAINT fk_timer_sessions_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
        )
    )SQL",
            &error)) {
        throw std::runtime_error("Failed to ensure timer_sessions schema: " + error);
    }

    if (!execute(
            R"SQL(
        CREATE UNIQUE INDEX IF NOT EXISTS idx_timer_sessions_single_active_per_user
        ON timer_sessions (user_id) WHERE is_active = TRUE
    )SQL",
            &error)) {
        throw std::runtime_error("Failed to ensure timer_sessions active index schema: " + error);
    }
}

bool Postgres::ping(std::string* error) const {
    ensureLoaded();
    ConnectionHandle handle(connection_string_);
    if (handle.conn == nullptr || libpq().PQstatus(handle.conn) != CONNECTION_OK) {
        if (error != nullptr) {
            *error = handle.conn == nullptr ? "failed to open connection" : connError(handle.conn);
        }
        return false;
    }

    PGresult* result = libpq().PQexec(handle.conn, "SELECT 1");
    if (result == nullptr) {
        if (error != nullptr) {
            *error = connError(handle.conn);
        }
        return false;
    }

    const bool ok = (libpq().PQresultStatus(result) == PGRES_TUPLES_OK ||
                     libpq().PQresultStatus(result) == PGRES_COMMAND_OK);
    if (!ok && error != nullptr) {
        *error = connError(handle.conn);
    }
    libpq().PQclear(result);
    return ok;
}

const std::string& Postgres::connectionString() const {
    return connection_string_;
}

bool Postgres::execute(const std::string& sql, std::string* error) const {
    ensureLoaded();
    ConnectionHandle handle(connection_string_);
    if (handle.conn == nullptr || libpq().PQstatus(handle.conn) != CONNECTION_OK) {
        if (error != nullptr) {
            *error = handle.conn == nullptr ? "failed to open connection" : connError(handle.conn);
        }
        return false;
    }

    PGresult* result = libpq().PQexec(handle.conn, sql.c_str());
    if (result == nullptr) {
        if (error != nullptr) {
            *error = connError(handle.conn);
        }
        return false;
    }

    const auto status = libpq().PQresultStatus(result);
    const bool ok = (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK);
    if (!ok && error != nullptr) {
        *error = connError(handle.conn);
    }
    libpq().PQclear(result);
    return ok;
}

std::optional<Postgres::Row> Postgres::queryOne(const std::string& sql, std::string* error) const {
    ensureLoaded();
    ConnectionHandle handle(connection_string_);
    if (handle.conn == nullptr || libpq().PQstatus(handle.conn) != CONNECTION_OK) {
        if (error != nullptr) {
            *error = handle.conn == nullptr ? "failed to open connection" : connError(handle.conn);
        }
        return std::nullopt;
    }

    PGresult* result = libpq().PQexec(handle.conn, sql.c_str());
    if (result == nullptr) {
        if (error != nullptr) {
            *error = connError(handle.conn);
        }
        return std::nullopt;
    }

    const auto status = libpq().PQresultStatus(result);
    if (status != PGRES_TUPLES_OK) {
        if (error != nullptr) {
            *error = connError(handle.conn);
        }
        libpq().PQclear(result);
        return std::nullopt;
    }

    if (libpq().PQntuples(result) <= 0) {
        libpq().PQclear(result);
        return std::nullopt;
    }

    Row row;
    const int fields = libpq().PQnfields(result);
    for (int i = 0; i < fields; ++i) {
        const char* name = libpq().PQfname(result, i);
        const char* value = libpq().PQgetvalue(result, 0, i);
        if (name != nullptr) {
            row.fields[name] = value != nullptr ? value : "";
        }
    }

    libpq().PQclear(result);
    return row;
}

std::vector<Postgres::Row> Postgres::queryAll(const std::string& sql, std::string* error) const {
    ensureLoaded();
    ConnectionHandle handle(connection_string_);
    if (handle.conn == nullptr || libpq().PQstatus(handle.conn) != CONNECTION_OK) {
        if (error != nullptr) {
            *error = handle.conn == nullptr ? "failed to open connection" : connError(handle.conn);
        }
        return {};
    }

    PGresult* result = libpq().PQexec(handle.conn, sql.c_str());
    if (result == nullptr) {
        if (error != nullptr) {
            *error = connError(handle.conn);
        }
        return {};
    }

    const auto status = libpq().PQresultStatus(result);
    if (status != PGRES_TUPLES_OK) {
        if (error != nullptr) {
            *error = connError(handle.conn);
        }
        libpq().PQclear(result);
        return {};
    }

    std::vector<Row> rows;
    const int tuples = libpq().PQntuples(result);
    const int fields = libpq().PQnfields(result);
    rows.reserve(static_cast<std::size_t>(tuples));
    for (int r = 0; r < tuples; ++r) {
        Row row;
        for (int c = 0; c < fields; ++c) {
            const char* name = libpq().PQfname(result, c);
            const char* value = libpq().PQgetvalue(result, r, c);
            if (name != nullptr) {
                row.fields[name] = value != nullptr ? value : "";
            }
        }
        rows.push_back(std::move(row));
    }

    libpq().PQclear(result);
    return rows;
}

std::string Postgres::escapeLiteral(std::string value) const {
    ensureLoaded();
    ConnectionHandle handle(connection_string_);
    if (handle.conn == nullptr || libpq().PQstatus(handle.conn) != CONNECTION_OK) {
        throw std::runtime_error("Failed to open connection for escaping literal.");
    }

    std::string escaped;
    escaped.resize(value.size() * 2 + 1);

    int error = 0;
    const auto written = libpq().PQescapeStringConn(
        handle.conn, escaped.data(), value.c_str(), value.size(), &error);
    if (error != 0) {
        throw std::runtime_error("Failed to escape SQL literal.");
    }

    escaped.resize(written);
    return escaped;
}

}  // namespace app::db

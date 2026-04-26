#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace app::db {

struct PostgresConfig {
    std::string host;
    int port{5432};
    std::string dbname;
    std::string user;
    std::string password;
    std::string sslmode{"disable"};

    [[nodiscard]] std::string connectionString() const;
};

class Postgres {
public:
    struct Row {
        std::unordered_map<std::string, std::string> fields;
    };

    explicit Postgres(PostgresConfig config);
    ~Postgres();

    void ensureSchema() const;
    [[nodiscard]] bool ping(std::string* error = nullptr) const;
    [[nodiscard]] const std::string& connectionString() const;
    [[nodiscard]] bool execute(const std::string& sql, std::string* error = nullptr) const;
    [[nodiscard]] std::optional<Row> queryOne(const std::string& sql, std::string* error = nullptr) const;
    [[nodiscard]] std::string escapeLiteral(std::string value) const;

private:
    void ensureLoaded() const;

    std::string connection_string_;
};

}  // namespace app::db

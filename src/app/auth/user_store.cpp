#include "app/auth/user_store.h"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace app::auth {

UserStore::UserStore(std::shared_ptr<db::Postgres> db) : db_(std::move(db)) {}

std::optional<User> UserStore::findByEmail(const std::string& email) const {
    const std::string escaped_email = db_->escapeLiteral(email);
    const std::string sql =
        "SELECT id, username, email, password_hash FROM users WHERE email = '" + escaped_email + "' LIMIT 1";
    auto row = db_->queryOne(sql);
    if (!row.has_value()) {
        return std::nullopt;
    }

    User user{
        .id = row->fields.at("id"),
        .username = row->fields.at("username"),
        .email = row->fields.at("email"),
        .password_hash = row->fields.at("password_hash"),
    };
    return user;
}

std::optional<User> UserStore::findById(const std::string& user_id) const {
    const std::string escaped_user_id = db_->escapeLiteral(user_id);
    const std::string sql =
        "SELECT id, username, email, password_hash FROM users WHERE id = '" + escaped_user_id + "' LIMIT 1";
    auto row = db_->queryOne(sql);
    if (!row.has_value()) {
        return std::nullopt;
    }

    User user{
        .id = row->fields.at("id"),
        .username = row->fields.at("username"),
        .email = row->fields.at("email"),
        .password_hash = row->fields.at("password_hash"),
    };
    return user;
}

bool UserStore::existsByEmail(const std::string& email) const {
    const std::string escaped_email = db_->escapeLiteral(email);
    const std::string sql = "SELECT 1 AS found FROM users WHERE email = '" + escaped_email + "' LIMIT 1";
    return db_->queryOne(sql).has_value();
}

User UserStore::create(std::string username, std::string email, std::string password_hash) {
    const std::string user_id = generateUserId();

    User user{
        .id = user_id,
        .username = std::move(username),
        .email = std::move(email),
        .password_hash = std::move(password_hash),
    };

    const std::string sql = "INSERT INTO users (id, username, email, password_hash) VALUES ('" +
                            db_->escapeLiteral(user.id) + "', '" +
                            db_->escapeLiteral(user.username) + "', '" +
                            db_->escapeLiteral(user.email) + "', '" +
                            db_->escapeLiteral(user.password_hash) + "')";
    std::string error;
    if (!db_->execute(sql, &error)) {
        throw std::runtime_error("Failed to create user: " + error);
    }

    return user;
}

std::string UserStore::generateUserId() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++sequence_;
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    return "user-" + std::to_string(now) + "-" + std::to_string(sequence_);
}

}  // namespace app::auth

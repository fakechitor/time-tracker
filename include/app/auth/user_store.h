#pragma once

#include <mutex>
#include <optional>
#include <memory>
#include <string>

#include "app/auth/models.h"
#include "app/db/postgres.h"

namespace app::auth {

class UserStore {
public:
    explicit UserStore(std::shared_ptr<db::Postgres> db);

    std::optional<User> findByEmail(const std::string& email) const;
    std::optional<User> findById(const std::string& user_id) const;
    bool existsByEmail(const std::string& email) const;
    User create(std::string username, std::string email, std::string password_hash);

private:
    std::string generateUserId();

    mutable std::mutex mutex_;
    std::shared_ptr<db::Postgres> db_;
    std::size_t sequence_{0};
};

}  // namespace app::auth

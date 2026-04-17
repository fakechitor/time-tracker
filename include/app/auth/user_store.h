#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "app/auth/models.h"

namespace app::auth {

class UserStore {
public:
    std::optional<User> findByEmail(const std::string& email) const;
    std::optional<User> findById(const std::string& user_id) const;
    bool existsByEmail(const std::string& email) const;
    User create(std::string username, std::string email, std::string password_hash);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, User> users_by_email_;
    std::unordered_map<std::string, User> users_by_id_;
    std::size_t sequence_{0};
};

}  // namespace app::auth


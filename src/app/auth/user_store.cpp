#include "app/auth/user_store.h"

namespace app::auth {

std::optional<User> UserStore::findByEmail(const std::string& email) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = users_by_email_.find(email);
    if (it == users_by_email_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<User> UserStore::findById(const std::string& user_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = users_by_id_.find(user_id);
    if (it == users_by_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool UserStore::existsByEmail(const std::string& email) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return users_by_email_.contains(email);
}

User UserStore::create(std::string username, std::string email, std::string password_hash) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++sequence_;
    User user{
        .id = "user-" + std::to_string(sequence_),
        .username = std::move(username),
        .email = std::move(email),
        .password_hash = std::move(password_hash),
    };
    users_by_id_[user.id] = user;
    users_by_email_[user.email] = user;
    return user;
}

}  // namespace app::auth


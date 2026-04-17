#pragma once

#include <crow.h>

namespace app::http {

inline crow::response json(int status, crow::json::wvalue body) {
    crow::response response{status};
    response.set_header("Content-Type", "application/json");
    response.body = body.dump();
    return response;
}

inline crow::response error(int status, const std::string& message) {
    crow::json::wvalue body;
    body["error"] = message;
    return json(status, std::move(body));
}

}  // namespace app::http


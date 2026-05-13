#include <qdb/client/request.h>

#include <nlohmann/json.hpp>

namespace qdb::client {

Request::Request(const std::string& method, const std::string& sql, const std::string& token)
    : method_(method), sql_(sql), token_(token) {}

std::string Request::ToJson() const {
    nlohmann::json j;
    j["method"] = method_;

    if (!sql_.empty()) {
        j["sql"] = sql_;
    }

    if (!token_.empty()) {
        j["token"] = token_;
    }

    if (request_id_.has_value()) {
        j["request_id"] = request_id_.value();
    }

    return j.dump() + "\n";
}

}  // namespace qdb::client

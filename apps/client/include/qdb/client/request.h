#ifndef QUASARDB_REQUEST_H
#define QUASARDB_REQUEST_H

#include <optional>
#include <string>

namespace qdb::client {

class Request {
public:
    Request() = default;
    Request(const std::string& method, const std::string& sql = "", const std::string& token = "");

    std::string ToJson() const;

    std::string method_;
    std::string sql_;
    std::string token_;
    std::optional<std::string> request_id_;
};

}  // namespace qdb::client

#endif  // QUASARDB_REQUEST_H

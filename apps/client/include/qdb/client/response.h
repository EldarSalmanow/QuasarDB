#ifndef QUASARDB_RESPONSE_H
#define QUASARDB_RESPONSE_H

#include <string>

namespace qdb::client {

class Response {
public:
    Response() = default;

    static Response FromJson(const std::string& json_str);

    bool IsError() const;
    bool IsSuccess() const;

    std::string GetStatus() const;
    std::string GetCode() const;
    std::string GetMessage() const;
    std::string GetData() const;

private:
    std::string status_;
    std::string code_;
    std::string message_;
    std::string data_;
};

}  // namespace qdb::client

#endif  // QUASARDB_RESPONSE_H

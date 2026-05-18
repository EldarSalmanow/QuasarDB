#ifndef QUASARDB_REQUEST_H
#define QUASARDB_REQUEST_H

#include <nlohmann/json.hpp>

#include <optional>
#include <string>


namespace qdb::core {

class Request {
private:
    friend class RequestBuilder;

public:
    Request();

    Request(std::string method, std::string sql, std::string token);

public:
    static auto FromJson(std::string string) -> Request;

    static auto FromJsonObject(const nlohmann::json &json) -> Request;

public:
    nlohmann::json ToJsonObject() const;

    std::string ToJson() const;

private:
    std::string method_;

    std::string sql_;

    std::string token_;

    std::optional<std::string> request_id_;
};

class RequestBuilder {
public:
    RequestBuilder();

public:
    static auto ExecuteQuery(std::string sql) -> RequestBuilder;

    static auto SubmitQuery(std::string sql) -> RequestBuilder;

    static auto GetStatus(std::string request_id) -> RequestBuilder;

public:
    auto Method(std::string method) -> RequestBuilder &;

    auto Sql(std::string sql) -> RequestBuilder &;

    auto Token(std::string token) -> RequestBuilder &;

    auto RequestId(std::string request_id) -> RequestBuilder &;

    auto Build() const -> Request;

private:
    Request request_;
};

}  // namespace qdb::core

#endif  // QUASARDB_REQUEST_H



#ifndef QUASARDB_RESPONSE_H
#define QUASARDB_RESPONSE_H

#include <nlohmann/json.hpp>

#include <string>


namespace qdb::core {

class Response {
private:
    friend class ResponseBuilder;

public:
    Response();

    Response(std::string status, std::string code, std::string message, std::string data);

public:
    static auto FromJson(std::string string) -> Response;

    static auto FromJsonObject(const nlohmann::json &json) -> Response;

public:
    auto ToJsonObject() const -> nlohmann::json;

    auto ToJson() const -> std::string;

    auto IsError() const -> bool;

    auto IsSuccess() const -> bool;

    auto GetStatus() const -> std::string;

    auto GetCode() const -> std::string;

    auto GetMessage() const -> std::string;

    auto GetData() const -> std::string;

private:
    std::string status_;

    std::string code_;

    std::string message_;

    std::string data_;
};

class ResponseBuilder {
public:
    ResponseBuilder();

public:
    static auto Ok() -> ResponseBuilder;

    static auto Error() -> ResponseBuilder;

public:
    auto Status(std::string status) -> ResponseBuilder &;

    auto Code(std::string code) -> ResponseBuilder &;

    auto Message(std::string message) -> ResponseBuilder &;

    auto Data(std::string data) -> ResponseBuilder &;

    auto Build() const -> Response;

private:
    Response response_;
};

}  // namespace qdb::core

#endif  // QUASARDB_RESPONSE_H




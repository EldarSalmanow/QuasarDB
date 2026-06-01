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

    Response(std::string status, std::string message, nlohmann::json data = nlohmann::json::object());

public:
    static auto FromJson(std::string string) -> Response;

    static auto FromJsonObject(const nlohmann::json& json) -> Response;

public:
    auto ToJson() const -> std::string;

    auto ToJsonObject() const -> nlohmann::json;

    auto IsError() const -> bool;

    auto IsSuccess() const -> bool;

    auto IsPending() const -> bool;

    auto GetMessage() const -> std::string;

    auto GetData() const -> std::string;

    auto GetDataObject() const -> const nlohmann::json&;

private:
    std::string status_;

    std::string message_;

    nlohmann::json data_ = nlohmann::json::object();
};

class ResponseBuilder {
public:
    ResponseBuilder();

public:
    static auto Success() -> ResponseBuilder;

    static auto Pending() -> ResponseBuilder;

    static auto Error() -> ResponseBuilder;

public:
    auto Status(std::string status) -> ResponseBuilder&;

    auto Message(std::string message) -> ResponseBuilder&;

    auto Data(nlohmann::json data) -> ResponseBuilder&;

    auto Build() const -> Response;

private:
    Response response_;
};

auto Success(std::string message, nlohmann::json data = nlohmann::json::object()) -> Response;

auto Pending(std::string message, nlohmann::json data = nlohmann::json::object()) -> Response;

auto Error(std::string message, nlohmann::json data = nlohmann::json::object()) -> Response;

auto SuccessJson(std::string message, nlohmann::json data = nlohmann::json::object()) -> nlohmann::json;

auto PendingJson(std::string message, nlohmann::json data = nlohmann::json::object()) -> nlohmann::json;

auto ErrorJson(std::string message, nlohmann::json data = nlohmann::json::object()) -> nlohmann::json;

}  // namespace qdb::core

#endif  // QUASARDB_RESPONSE_H

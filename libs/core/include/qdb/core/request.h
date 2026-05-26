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

    Request(std::string action, nlohmann::json data = nlohmann::json::object(), std::string token = {});

public:
    static auto FromJson(std::string string) -> Request;

    static auto FromJsonObject(const nlohmann::json &json) -> Request;

public:
    auto ToJson() const -> std::string;

    auto ToJsonObject() const -> nlohmann::json;

    auto Action() const -> const std::string &;

    auto Data() const -> const nlohmann::json &;

    auto Query() const -> std::string;

    auto TaskId() const -> std::optional<std::string>;

    auto Token() const -> const std::string &;

private:
    std::string action_;

    std::string token_;

    nlohmann::json data_ = nlohmann::json::object();
};

class RequestBuilder {
public:
    RequestBuilder();

public:
    static auto Login(std::string username, std::string password) -> RequestBuilder;

    static auto CreateSuperuser(std::string username, std::string password) -> RequestBuilder;

    static auto Handshake() -> RequestBuilder;

    static auto Query(std::string sql) -> RequestBuilder;

    static auto CheckTask(std::string task_id) -> RequestBuilder;

public:
    auto Action(std::string action) -> RequestBuilder &;

    auto Data(nlohmann::json data) -> RequestBuilder &;

    auto Token(std::string token) -> RequestBuilder &;

    auto TaskId(std::string task_id) -> RequestBuilder &;

    auto Build() const -> Request;

private:
    Request request_;
};

}  // namespace qdb::core

#endif  // QUASARDB_REQUEST_H

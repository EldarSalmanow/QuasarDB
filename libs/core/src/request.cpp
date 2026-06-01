#include <qdb/core/request.h>

#include <nlohmann/json.hpp>

namespace qdb::core {

Request::Request() = default;

Request::Request(std::string action, nlohmann::json data, std::string token)
    : action_(std::move(action)), token_(std::move(token)), data_(std::move(data)) {}

auto Request::FromJson(std::string string) -> Request {
    try {
        return FromJsonObject(nlohmann::json::parse(std::move(string)));
    } catch (const std::exception&) {
        return Request{};
    }
}

auto Request::FromJsonObject(const nlohmann::json& json) -> Request {
    Request request;

    if (json.contains("action")) {
        request.action_ = json["action"].get<std::string>();
    }

    if (json.contains("data") && json["data"].is_object()) {
        request.data_ = json["data"];
    }

    if (json.contains("token")) {
        request.token_ = json["token"].get<std::string>();
    }

    return request;
}

auto Request::ToJsonObject() const -> nlohmann::json {
    nlohmann::json json;

    json["action"] = action_;

    if (!token_.empty()) {
        json["token"] = token_;
    }

    json["data"] = data_;

    return json;
}

auto Request::ToJson() const -> std::string { return ToJsonObject().dump(); }

auto Request::Action() const -> const std::string& { return action_; }

auto Request::Data() const -> const nlohmann::json& { return data_; }

auto Request::Query() const -> std::string { return data_.value("query", ""); }

auto Request::Token() const -> const std::string& { return token_; }

auto Request::TaskId() const -> std::optional<std::string> {
    if (!data_.contains("task_id") || !data_["task_id"].is_string()) {
        return std::nullopt;
    }

    return data_["task_id"].get<std::string>();
}

RequestBuilder::RequestBuilder() = default;

auto RequestBuilder::Login(std::string username, std::string password) -> RequestBuilder {
    return RequestBuilder{}.Action("login").Data({{"username", std::move(username)}, {"password", std::move(password)}}
    );
}

auto RequestBuilder::CreateSuperuser(std::string username, std::string password) -> RequestBuilder {
    return RequestBuilder{}
        .Action("login")
        .Data({{"username", std::move(username)}, {"password", std::move(password)}, {"create", true}});
}

auto RequestBuilder::Handshake() -> RequestBuilder { return RequestBuilder{}.Action("handshake"); }

auto RequestBuilder::Query(std::string sql) -> RequestBuilder {
    return RequestBuilder{}.Action("query").Data({{"query", std::move(sql)}});
}

auto RequestBuilder::CheckTask(std::string task_id) -> RequestBuilder {
    return RequestBuilder{}.Action("check_task").TaskId(std::move(task_id));
}

auto RequestBuilder::Telemetry() -> RequestBuilder { return RequestBuilder{}.Action("telemetry"); }

auto RequestBuilder::Action(std::string action) -> RequestBuilder& {
    request_.action_ = std::move(action);

    return *this;
}

auto RequestBuilder::Data(nlohmann::json data) -> RequestBuilder& {
    request_.data_ = std::move(data);

    return *this;
}

auto RequestBuilder::Token(std::string token) -> RequestBuilder& {
    request_.token_ = std::move(token);

    return *this;
}

auto RequestBuilder::TaskId(std::string task_id) -> RequestBuilder& {
    request_.data_["task_id"] = std::move(task_id);

    return *this;
}

auto RequestBuilder::Build() const -> Request { return request_; }

}  // namespace qdb::core

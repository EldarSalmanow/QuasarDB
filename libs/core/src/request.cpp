#include <qdb/core/request.h>

#include <nlohmann/json.hpp>


namespace qdb::core {

Request::Request() = default;

Request::Request(std::string method, std::string sql, std::string token)
        : method_(std::move(method)), sql_(std::move(sql)), token_(std::move(token)) {}

auto Request::FromJson(std::string string) -> Request {
    try {
        return FromJsonObject(nlohmann::json::parse(std::move(string)));
    } catch (const std::exception &) {
        return Request {};
    }
}

auto Request::FromJsonObject(const nlohmann::json &json) -> Request {
    Request request;

    if (json.contains("method")) {
        request.method_ = json["method"].get<std::string>();
    }

    if (json.contains("sql")) {
        request.sql_ = json["sql"].get<std::string>();
    }

    if (json.contains("token")) {
        request.token_ = json["token"].get<std::string>();
    }

    if (json.contains("request_id")) {
        request.request_id_ = json["request_id"].get<std::string>();
    }

    return request;
}

auto Request::ToJsonObject() const -> nlohmann::json {
    nlohmann::json json;

    json["method"] = method_;

    if (!sql_.empty()) {
        json["sql"] = sql_;
    }

    if (!token_.empty()) {
        json["token"] = token_;
    }

    if (request_id_.has_value()) {
        json["request_id"] = request_id_.value();
    }

    return json;
}

auto Request::ToJson() const -> std::string {
    return ToJsonObject().dump() + "\n";
}

auto Request::Method() const -> const std::string & { return method_; }

auto Request::Sql() const -> const std::string & { return sql_; }

auto Request::Token() const -> const std::string & { return token_; }

auto Request::RequestId() const -> const std::optional<std::string> & { return request_id_; }

RequestBuilder::RequestBuilder() = default;

auto RequestBuilder::ExecuteQuery(std::string sql) -> RequestBuilder {
    return RequestBuilder{}.Method("ExecuteQuery").Sql(std::move(sql));
}

auto RequestBuilder::SubmitQuery(std::string sql) -> RequestBuilder {
    return RequestBuilder{}.Method("SubmitQuery").Sql(std::move(sql));
}

auto RequestBuilder::GetStatus(std::string request_id) -> RequestBuilder {
    return RequestBuilder{}.Method("GetStatus").RequestId(std::move(request_id));
}

auto RequestBuilder::Method(std::string method) -> RequestBuilder & {
    request_.method_ = std::move(method);

    return *this;
}

auto RequestBuilder::Sql(std::string sql) -> RequestBuilder & {
    request_.sql_ = std::move(sql);

    return *this;
}

auto RequestBuilder::Token(std::string token) -> RequestBuilder & {
    request_.token_ = std::move(token);

    return *this;
}

auto RequestBuilder::RequestId(std::string request_id) -> RequestBuilder & {
    request_.request_id_ = std::move(request_id);

    return *this;
}

auto RequestBuilder::Build() const -> Request {
    return request_;
}

}  // namespace qdb::core

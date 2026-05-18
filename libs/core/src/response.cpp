#include <qdb/core/response.h>

#include <nlohmann/json.hpp>


namespace qdb::core {

Response::Response() = default;

Response::Response(std::string status, std::string code, std::string message, std::string data)
        : status_(std::move(status)), code_(std::move(code)), message_(std::move(message)), data_(std::move(data)) {}

auto Response::FromJson(std::string string) -> Response {
    try {
        return FromJsonObject(nlohmann::json::parse(std::move(string)));
    } catch (const std::exception &exception) {
        return ResponseBuilder::Error()
            .Code("PARSE_ERROR")
            .Message(std::string("Failed to parse JSON: ") + exception.what())
            .Build();
    }
}

auto Response::FromJsonObject(const nlohmann::json &json) -> Response {
    Response response;

    try {
        if (json.contains("status")) {
            response.status_ = json["status"].get<std::string>();
        }

        if (json.contains("code")) {
            response.code_ = json["code"].get<std::string>();
        }

        if (json.contains("message")) {
            response.message_ = json["message"].get<std::string>();
        }

        if (json.contains("data")) {
            if (json["data"].is_string()) {
                response.data_ = json["data"].get<std::string>();
            } else {
                response.data_ = json["data"].dump();
            }
        }
    } catch (const std::exception &exception) {
        return ResponseBuilder::Error()
            .Code("PARSE_ERROR")
            .Message(std::string("Failed to parse response object: ") + exception.what())
            .Build();
    }

    return response;
}

auto Response::ToJsonObject() const -> nlohmann::json {
    return nlohmann::json{
        {"status", status_},
        {"code", code_},
        {"message", message_},
        {"data", data_}
    };
}

auto Response::ToJson() const -> std::string {
    return ToJsonObject().dump();
}

auto Response::IsError() const -> bool { return status_ == "error"; }

auto Response::IsSuccess() const -> bool { return status_ == "ok"; }

auto Response::GetStatus() const -> std::string { return status_; }

auto Response::GetCode() const -> std::string { return code_; }

auto Response::GetMessage() const -> std::string { return message_; }

auto Response::GetData() const -> std::string { return data_; }

ResponseBuilder::ResponseBuilder() = default;

auto ResponseBuilder::Ok() -> ResponseBuilder {
    return ResponseBuilder{}.Status("ok");
}

auto ResponseBuilder::Error() -> ResponseBuilder {
    return ResponseBuilder{}.Status("error");
}

auto ResponseBuilder::Status(std::string status) -> ResponseBuilder & {
    response_.status_ = std::move(status);

    return *this;
}

auto ResponseBuilder::Code(std::string code) -> ResponseBuilder & {
    response_.code_ = std::move(code);

    return *this;
}

auto ResponseBuilder::Message(std::string message) -> ResponseBuilder & {
    response_.message_ = std::move(message);

    return *this;
}

auto ResponseBuilder::Data(std::string data) -> ResponseBuilder & {
    response_.data_ = std::move(data);

    return *this;
}

auto ResponseBuilder::Build() const -> Response {
    return response_;
}

}  // namespace qdb::core



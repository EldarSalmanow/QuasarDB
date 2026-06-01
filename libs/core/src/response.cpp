#include <qdb/core/response.h>

#include <nlohmann/json.hpp>

namespace qdb::core {

Response::Response() = default;

Response::Response(std::string status, std::string message, nlohmann::json data)
    : status_(std::move(status)), message_(std::move(message)), data_(std::move(data)) {}

auto Response::FromJson(std::string string) -> Response {
    try {
        return FromJsonObject(nlohmann::json::parse(std::move(string)));
    } catch (const std::exception& exception) {
        return ResponseBuilder::Error().Message(std::string("Failed to parse JSON: ") + exception.what()).Build();
    }
}

auto Response::FromJsonObject(const nlohmann::json& json) -> Response {
    Response response;

    try {
        if (json.contains("status")) {
            response.status_ = json["status"].get<std::string>();
        }

        if (json.contains("message")) {
            response.message_ = json["message"].get<std::string>();
        }

        if (json.contains("data")) {
            response.data_ = json["data"];
        }
    } catch (const std::exception& exception) {
        return ResponseBuilder::Error()
            .Message(std::string("Failed to parse response object: ") + exception.what())
            .Build();
    }

    return response;
}

auto Response::ToJson() const -> std::string { return ToJsonObject().dump(); }

auto Response::ToJsonObject() const -> nlohmann::json {
    auto json = nlohmann::json{{"status", status_}, {"message", message_}, {"data", data_}};

    return json;
}

auto Response::IsError() const -> bool { return status_ == "error"; }

auto Response::IsSuccess() const -> bool { return status_ == "success"; }

auto Response::IsPending() const -> bool { return status_ == "pending"; }

auto Response::GetMessage() const -> std::string { return message_; }

auto Response::GetData() const -> std::string {
    if (data_.is_string()) {
        return data_.get<std::string>();
    }

    return data_.dump();
}

auto Response::GetDataObject() const -> const nlohmann::json& { return data_; }

ResponseBuilder::ResponseBuilder() = default;

auto ResponseBuilder::Success() -> ResponseBuilder { return ResponseBuilder{}.Status("success"); }

auto ResponseBuilder::Pending() -> ResponseBuilder { return ResponseBuilder{}.Status("pending"); }

auto ResponseBuilder::Error() -> ResponseBuilder { return ResponseBuilder{}.Status("error"); }

auto ResponseBuilder::Status(std::string status) -> ResponseBuilder& {
    response_.status_ = std::move(status);

    return *this;
}

auto ResponseBuilder::Message(std::string message) -> ResponseBuilder& {
    response_.message_ = std::move(message);

    return *this;
}

auto ResponseBuilder::Data(nlohmann::json data) -> ResponseBuilder& {
    response_.data_ = std::move(data);

    return *this;
}

auto ResponseBuilder::Build() const -> Response { return response_; }

auto Success(std::string message, nlohmann::json data) -> Response {
    return ResponseBuilder::Success().Message(std::move(message)).Data(std::move(data)).Build();
}

auto Pending(std::string message, nlohmann::json data) -> Response {
    return ResponseBuilder::Pending().Message(std::move(message)).Data(std::move(data)).Build();
}

auto Error(std::string message, nlohmann::json data) -> Response {
    return ResponseBuilder::Error().Message(std::move(message)).Data(std::move(data)).Build();
}

auto SuccessJson(std::string message, nlohmann::json data) -> nlohmann::json {
    return Success(std::move(message), std::move(data)).ToJsonObject();
}

auto PendingJson(std::string message, nlohmann::json data) -> nlohmann::json {
    return Pending(std::move(message), std::move(data)).ToJsonObject();
}

auto ErrorJson(std::string message, nlohmann::json data) -> nlohmann::json {
    return Error(std::move(message), std::move(data)).ToJsonObject();
}

}  // namespace qdb::core

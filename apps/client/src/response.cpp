#include <qdb/client/response.h>

#include <nlohmann/json.hpp>

namespace qdb::client {

Response Response::FromJson(const std::string& json_str) {
    Response response;

    try {
        auto j = nlohmann::json::parse(json_str);

        if (j.contains("status")) {
            response.status_ = j["status"].get<std::string>();
        }

        if (j.contains("code")) {
            response.code_ = j["code"].get<std::string>();
        }

        if (j.contains("message")) {
            response.message_ = j["message"].get<std::string>();
        }

        if (j.contains("data")) {
            if (j["data"].is_string()) {
                response.data_ = j["data"].get<std::string>();
            } else {
                response.data_ = j["data"].dump();
            }
        }
    } catch (const std::exception& e) {
        response.status_ = "error";
        response.code_ = "PARSE_ERROR";
        response.message_ = std::string("Failed to parse JSON: ") + e.what();
        response.data_ = "";
    }

    return response;
}

bool Response::IsError() const {
    return status_ == "error";
}

bool Response::IsSuccess() const {
    return status_ == "success";
}

std::string Response::GetStatus() const {
    return status_;
}

std::string Response::GetCode() const {
    return code_;
}

std::string Response::GetMessage() const {
    return message_;
}

std::string Response::GetData() const {
    return data_;
}

}  // namespace qdb::client

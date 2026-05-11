#include <qdb/client/connection.h>

#include <nlohmann/json.hpp>

namespace qdb::client {

namespace {

Response MakeErrorResponse(const std::string& code, const std::string& message) {
    nlohmann::json j = {
        {"status", "error"},
        {"code", code},
        {"message", message},
        {"data", ""}
    };
    return Response::FromJson(j.dump());
}

}  // namespace

Connection::Connection(const std::string& host, uint32_t port)
    : client_(qdb::core::TcpClient::New(host, port)), host_(host), port_(port) {}

bool Connection::Connect() {
    if (!client_) {
        return false;
    }
    return client_->Connect();
}

void Connection::Disconnect() {
    if (client_) {
        client_->Disconnect();
    }
}

bool Connection::IsConnected() const {
    return client_ && client_->IsConnected();
}

Response Connection::ExecuteQuery(const std::string& sql, const std::string& token) {
    Request request("ExecuteQuery", sql, token);
    return SendRequest(request);
}

Response Connection::SendRequest(const Request& request) {
    if (!IsConnected()) {
        return MakeErrorResponse("NOT_CONNECTED", "Not connected to server");
    }

    try {
        std::string request_json = request.ToJson();
        nlohmann::json j = nlohmann::json::parse(request_json);

        if (!client_->Send(j)) {
            return MakeErrorResponse("SEND_FAILED", "Failed to send request");
        }

        auto response_json = client_->Receive();
        if (!response_json.has_value()) {
            return MakeErrorResponse("RECEIVE_FAILED", "Failed to receive response");
        }

        std::string response_str = response_json.value().dump();
        return Response::FromJson(response_str);

    } catch (const std::exception& e) {
        return MakeErrorResponse("EXCEPTION", e.what());
    }
}

}  // namespace qdb::client

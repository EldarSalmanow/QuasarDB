#include <qdb/client/connection.h>

#include <nlohmann/json.hpp>

namespace qdb::client {

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
        Response error_response;
        return Response::FromJson(R"({"status":"error","code":"NOT_CONNECTED","message":"Not connected to server","data":""})");
    }

    try {
        std::string request_json = request.ToJson();
        nlohmann::json j = nlohmann::json::parse(request_json);
        
        if (!client_->Send(j)) {
            return Response::FromJson(R"({"status":"error","code":"SEND_FAILED","message":"Failed to send request","data":""})");
        }

        auto response_json = client_->Receive();
        if (!response_json.has_value()) {
            return Response::FromJson(R"({"status":"error","code":"RECEIVE_FAILED","message":"Failed to receive response","data":""})");
        }

        std::string response_str = response_json.value().dump();
        return Response::FromJson(response_str);

    } catch (const std::exception& e) {
        std::string error_msg = R"({"status":"error","code":"EXCEPTION","message":")" + std::string(e.what()) + R"(","data":""})";
        return Response::FromJson(error_msg);
    }
}

}  // namespace qdb::client
#ifndef QUASARDB_TCP_CLIENT_H
#define QUASARDB_TCP_CLIENT_H

#include <nlohmann/json.hpp>

#include <qdb/core/request.h>
#include <qdb/core/response.h>

#include <optional>
#include <memory>
#include <string>

namespace qdb::core {

class TcpClient {
private:
    class TcpClientImpl;

public:
    TcpClient(const std::string& host, std::uint32_t port);

    // Internal constructor for accepted connections
    TcpClient(int socket, const std::string& host, std::uint32_t port);

public:
    TcpClient(const TcpClient& client) = delete;

    TcpClient(TcpClient&& client) noexcept;

public:
    ~TcpClient();

public:
    static auto New(const std::string& host, std::uint32_t port) -> std::unique_ptr<TcpClient>;

public:
    auto Connect() -> bool;

    auto Disconnect() -> void;

    auto Send(const nlohmann::json& request) -> bool;

    auto SendRequest(const Request& request) -> bool;

    auto SendResponse(const Response& response) -> bool;

    auto Receive() -> std::optional<nlohmann::json>;

    auto ReceiveRequest() -> std::optional<Request>;

    auto ReceiveResponse() -> std::optional<Response>;

    auto IsConnected() const -> bool;

public:
    auto Host() const -> const std::string&;

    auto Port() const -> std::uint32_t;

public:
    auto operator=(const TcpClient& client) -> TcpClient& = delete;

    auto operator=(TcpClient&& client) noexcept -> TcpClient&;

private:
    std::unique_ptr<TcpClientImpl> impl_;
};

};  // namespace qdb::core

#endif  // QUASARDB_TCP_CLIENT_H

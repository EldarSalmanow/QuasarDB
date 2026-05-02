#include <arpa/inet.h>

#include <qdb/core/socket.h>
#include <qdb/core/tcp_client.h>

#include <cstdint>
#include <limits>

namespace qdb::core {

class TcpClient::TcpClientImpl {
public:
    TcpClientImpl(std::string host, std::uint32_t port) : host_(std::move(host)), port_(port) {}

    TcpClientImpl(int socket, std::string host, std::uint32_t port)
        : host_(std::move(host)), port_(port), socket_(socket) {}

public:
    ~TcpClientImpl() { Disconnect(); }

public:
    auto Connect() -> bool {
        if (IsConnected()) {
            return false;
        }

        return socket_.Connect(host_, port_);
    }

    auto Disconnect() -> void { socket_.Disconnect(); }

    auto Send(const nlohmann::json& request) -> bool {
        if (!IsConnected()) {
            return false;
        }

        std::string request_str = request.dump();

        if (request_str.size() > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }

        std::uint32_t payload_size = htonl(static_cast<std::uint32_t>(request_str.size()));

        if (!socket_.Send(reinterpret_cast<const std::uint8_t*>(&payload_size), sizeof(payload_size))) {
            return false;
        }

        if (request_str.empty()) {
            return true;
        }

        return socket_.Send(reinterpret_cast<const std::uint8_t*>(request_str.data()), request_str.size());
    }

    auto Receive() -> std::optional<nlohmann::json> {
        if (!IsConnected()) {
            return std::nullopt;
        }

        std::uint32_t payload_size = 0;

        if (!socket_.Recv(reinterpret_cast<std::uint8_t*>(&payload_size), sizeof(payload_size))) {
            return std::nullopt;
        }

        std::uint32_t payload_size_host = ntohl(payload_size);

        if (payload_size_host == 0) {
            return std::nullopt;
        }

        std::string buffer(payload_size_host, '\0');

        if (!socket_.Recv(reinterpret_cast<std::uint8_t*>(buffer.data()), buffer.size())) {
            return std::nullopt;
        }

        try {
            return nlohmann::json::parse(buffer);
        } catch (const nlohmann::json::parse_error&) {
            return std::nullopt;
        }
    }

    auto IsConnected() const -> bool { return socket_.IsValid(); }

public:
    auto Host() const -> const std::string& { return host_; }

    auto Port() const -> std::uint32_t { return port_; }

private:
    std::string host_;
    std::uint32_t port_;

    Socket socket_;
};

TcpClient::TcpClient(const std::string& host, std::uint32_t port)
    : impl_(std::make_unique<TcpClientImpl>(host, port)) {}

TcpClient::TcpClient(int socket, const std::string& host, std::uint32_t port)
    : impl_(std::make_unique<TcpClientImpl>(socket, host, port)) {}

TcpClient::TcpClient(TcpClient&& client) noexcept : impl_(std::move(client.impl_)) {}

TcpClient::~TcpClient() = default;

auto TcpClient::New(const std::string& host, std::uint32_t port) -> std::unique_ptr<TcpClient> {
    return std::make_unique<TcpClient>(host, port);
}

auto TcpClient::Connect() -> bool { return impl_->Connect(); }

auto TcpClient::Disconnect() -> void { impl_->Disconnect(); }

auto TcpClient::Send(const nlohmann::json& request) -> bool { return impl_->Send(request); }

auto TcpClient::Receive() -> std::optional<nlohmann::json> { return impl_->Receive(); }

auto TcpClient::IsConnected() const -> bool { return impl_->IsConnected(); }

auto TcpClient::Host() const -> const std::string& { return impl_->Host(); }

auto TcpClient::Port() const -> std::uint32_t { return impl_->Port(); }

auto TcpClient::operator=(TcpClient&& client) noexcept -> TcpClient& {
    if (this == &client) {
        return *this;
    }

    impl_ = std::move(client.impl_);

    return *this;
}

};  // namespace qdb::core

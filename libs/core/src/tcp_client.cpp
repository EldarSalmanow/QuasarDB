#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <qdb/core/tcp_client.h>

#include <cstdint>

auto SendAll(int socket, const std::uint8_t* data, std::size_t size) -> bool {
    std::size_t total_sent = 0;

    while (total_sent < size) {
        ssize_t bytes_sent = ::send(socket, data + total_sent, size - total_sent, 0);

        if (bytes_sent <= 0) {
            return false;
        }

        total_sent += static_cast<std::size_t>(bytes_sent);
    }

    return true;
}

auto RecvAll(int socket, std::uint8_t* data, std::size_t size) -> bool {
    std::size_t total_received = 0;

    while (total_received < size) {
        ssize_t bytes_received = ::recv(socket, data + total_received, size - total_received, 0);

        if (bytes_received <= 0) {
            return false;
        }

        total_received += static_cast<std::size_t>(bytes_received);
    }

    return true;
}

namespace qdb::core {

class TcpClient::TcpClientImpl {
public:
    TcpClientImpl(const std::string& host, std::uint32_t port) : host_(host), port_(port), socket_(-1) {}

    TcpClientImpl(int socket, const std::string& host, std::uint32_t port)
        : host_(host), port_(port), socket_(socket) {}

public:
    ~TcpClientImpl() { Disconnect(); }

public:
    auto Connect() -> bool {
        if (IsConnected()) {
            return false;
        }

        socket_ = ::socket(AF_INET, SOCK_STREAM, 0);

        if (socket_ < 0) {
            return false;
        }

        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port = htons(port_);
        if (inet_pton(AF_INET, host_.c_str(), &address.sin_addr) <= 0) {
            ::close(socket_);

            socket_ = -1;

            return false;
        }

        if (::connect(socket_, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
            ::close(socket_);

            socket_ = -1;

            return false;
        }

        return true;
    }

    auto Disconnect() -> void {
        if (!IsConnected()) {
            return;
        }

        ::close(socket_);

        socket_ = -1;
    }

    auto Send(const nlohmann::json& request) -> bool {
        if (!IsConnected()) {
            return false;
        }

        std::string request_str = request.dump();

        if (request_str.size() > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }

        std::uint32_t payload_size = htonl(static_cast<std::uint32_t>(request_str.size()));

        if (!SendAll(socket_, reinterpret_cast<const std::uint8_t*>(&payload_size), sizeof(payload_size))) {
            return false;
        }

        if (request_str.empty()) {
            return true;
        }

        return SendAll(socket_, reinterpret_cast<const std::uint8_t*>(request_str.data()), request_str.size());
    }

    auto Receive() -> std::optional<nlohmann::json> {
        if (!IsConnected()) {
            return std::nullopt;
        }

        std::uint32_t payload_size = 0;

        if (!RecvAll(socket_, reinterpret_cast<std::uint8_t*>(&payload_size), sizeof(payload_size))) {
            return std::nullopt;
        }

        std::uint32_t payload_size_host = ntohl(payload_size);

        if (payload_size_host == 0) {
            return std::nullopt;
        }

        std::string buffer(payload_size_host, '\0');

        if (!RecvAll(socket_, reinterpret_cast<std::uint8_t*>(buffer.data()), buffer.size())) {
            return std::nullopt;
        }

        try {
            return nlohmann::json::parse(buffer);
        } catch (const nlohmann::json::parse_error&) {
            return std::nullopt;
        }
    }

    auto IsConnected() const -> bool { return socket_ >= 0; }

public:
    auto Host() const -> const std::string& { return host_; }

    auto Port() const -> std::uint32_t { return port_; }

private:
    std::string host_;
    std::uint32_t port_;

    int socket_;
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

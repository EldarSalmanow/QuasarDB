#include <qdb/core/socket.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace qdb::core {

Socket::Socket() : socket_(-1) {}

Socket::Socket(int socket) : socket_(socket) {}

Socket::Socket(Socket&& other) noexcept : socket_(std::exchange(other.socket_, -1)) {}

Socket::~Socket() { Reset(); }

auto Socket::Get() const -> int { return socket_; }

auto Socket::IsValid() const -> bool { return socket_ >= 0; }

auto Socket::Reset(int socket) -> void {
    if (socket_ >= 0) {
        ::close(socket_);
    }

    socket_ = socket;
}

auto Socket::Release() -> int { return std::exchange(socket_, -1); }

auto Socket::Send(const std::uint8_t* data, std::size_t size) const -> bool {
    if (socket_ < 0) {
        return false;
    }

    std::size_t total_sent = 0;

    while (total_sent < size) {
        ssize_t bytes_sent = ::send(socket_, data + total_sent, size - total_sent, 0);

        if (bytes_sent <= 0) {
            return false;
        }

        total_sent += static_cast<std::size_t>(bytes_sent);
    }

    return true;
}

auto Socket::Recv(std::uint8_t* data, std::size_t size) const -> bool {
    if (socket_ < 0) {
        return false;
    }

    std::size_t total_received = 0;

    while (total_received < size) {
        ssize_t bytes_received = ::recv(socket_, data + total_received, size - total_received, 0);

        if (bytes_received <= 0) {
            return false;
        }

        total_received += static_cast<std::size_t>(bytes_received);
    }

    return true;
}

auto Socket::Connect(const std::string& host, std::uint32_t port) -> bool {
    if (IsValid()) {
        return false;
    }

    int new_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (new_socket < 0) {
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) <= 0) {
        ::close(new_socket);
        return false;
    }

    if (::connect(new_socket, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(new_socket);
        return false;
    }

    Reset(new_socket);
    return true;
}

auto Socket::Disconnect() -> void { Reset(); }

auto Socket::operator=(Socket&& other) noexcept -> Socket& {
    if (this == &other) {
        return *this;
    }

    Reset();

    socket_ = std::exchange(other.socket_, -1);

    return *this;
}

}  // namespace qdb::core

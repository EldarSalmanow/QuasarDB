#ifndef QUASARDB_SOCKET_H
#define QUASARDB_SOCKET_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace qdb::core {

class Socket {
public:
    Socket();

    explicit Socket(int socket);

public:
    Socket(const Socket&) = delete;

    Socket(Socket&& other) noexcept;

public:
    ~Socket();

public:
    auto Get() const -> int;

    auto IsValid() const -> bool;

    auto Reset(int socket = -1) -> void;

    auto Release() -> int;

    auto Send(const std::uint8_t* data, std::size_t size) const -> bool;

    auto Recv(std::uint8_t* data, std::size_t size) const -> bool;

    auto Connect(const std::string& host, std::uint32_t port) -> bool;

    auto Disconnect() -> void;

public:
    auto operator=(const Socket&) -> Socket& = delete;

    auto operator=(Socket&& other) noexcept -> Socket&;

private:
    int socket_;
};

}  // namespace qdb::core

#endif  // QUASARDB_SOCKET_H

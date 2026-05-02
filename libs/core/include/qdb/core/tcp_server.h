#ifndef QUASARDB_TCP_SERVER_H
#define QUASARDB_TCP_SERVER_H

#include <qdb/core/tcp_client.h>

namespace qdb::core {

class TcpServer {
private:
    class TcpServerImpl;

public:
    TcpServer(const std::string& host, std::uint32_t port);

public:
    TcpServer(const TcpServer& server) = delete;

    TcpServer(TcpServer&& server) noexcept;

public:
    ~TcpServer();

public:
    static auto New(const std::string& host, std::uint32_t port) -> std::unique_ptr<TcpServer>;

public:
    auto Start() -> bool;

    auto Stop() -> void;

    auto Accept() -> std::unique_ptr<TcpClient>;

    auto IsRunning() const -> bool;

public:
    auto Host() const -> const std::string&;

    auto Port() const -> std::uint32_t;

public:
    auto operator=(const TcpServer& server) -> TcpServer& = delete;

    auto operator=(TcpServer&& server) noexcept -> TcpServer&;

private:
    std::unique_ptr<TcpServerImpl> impl_;
};

};  // namespace qdb::core

#endif  // QUASARDB_TCP_SERVER_H

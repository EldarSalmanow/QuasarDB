#include <arpa/inet.h>
#include <sys/socket.h>

#include <qdb/core/socket.h>
#include <qdb/core/tcp_server.h>

namespace qdb::core {

class TcpServer::TcpServerImpl {
public:
    TcpServerImpl(const std::string& host, std::uint32_t port) : host_(host), port_(port), running_(false) {}

public:
    ~TcpServerImpl() { Stop(); }

public:
    auto Start() -> bool {
        if (running_) {
            return false;
        }

        Socket socket(::socket(AF_INET, SOCK_STREAM, 0));

        if (!socket.IsValid()) {
            return false;
        }

        int opt = 1;
        if (::setsockopt(socket.Get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            return false;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port_);
        if (inet_pton(AF_INET, host_.c_str(), &address.sin_addr) <= 0) {
            return false;
        }

        if (::bind(socket.Get(), reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
            return false;
        }

        if (::listen(socket.Get(), 10) < 0) {
            return false;
        }

        socket_ = std::move(socket);
        running_ = true;

        return true;
    }

    auto Stop() -> void {
        if (!running_) {
            return;
        }

        running_ = false;

        socket_.Reset();
    }

    auto Accept() -> std::unique_ptr<TcpClient> {
        if (!running_) {
            return nullptr;
        }

        sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);

        int client_socket = ::accept(socket_.Get(), reinterpret_cast<struct sockaddr*>(&client_address), &client_len);

        if (client_socket < 0) {
            return nullptr;
        }

        char client_host[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_host, INET_ADDRSTRLEN);
        std::uint32_t client_port = ntohs(client_address.sin_port);

        return std::make_unique<TcpClient>(client_socket, client_host, client_port);
    }

    auto IsRunning() const -> bool { return running_; }

public:
    auto Host() const -> const std::string& { return host_; }

    auto Port() const -> std::uint32_t { return port_; }

private:
    std::string host_;
    std::uint32_t port_;

    Socket socket_;
    bool running_;
};

TcpServer::TcpServer(const std::string& host, std::uint32_t port)
    : impl_(std::make_unique<TcpServerImpl>(host, port)) {}

TcpServer::TcpServer(TcpServer&& server) noexcept : impl_(std::move(server.impl_)) {}

TcpServer::~TcpServer() = default;

auto TcpServer::New(const std::string& host, std::uint32_t port) -> std::unique_ptr<TcpServer> {
    return std::make_unique<TcpServer>(host, port);
}

auto TcpServer::Start() -> bool { return impl_->Start(); }

auto TcpServer::Stop() -> void { impl_->Stop(); }

auto TcpServer::Accept() -> std::unique_ptr<TcpClient> { return impl_->Accept(); }

auto TcpServer::IsRunning() const -> bool { return impl_->IsRunning(); }

auto TcpServer::Host() const -> const std::string& { return impl_->Host(); }

auto TcpServer::Port() const -> std::uint32_t { return impl_->Port(); }

auto TcpServer::operator=(TcpServer&& server) noexcept -> TcpServer& {
    if (this == &server) {
        return *this;
    }

    impl_ = std::move(server.impl_);

    return *this;
}

};  // namespace qdb::core

#include <gtest/gtest.h>

#include <qdb/core/tcp_client.h>
#include <qdb/core/tcp_server.h>

#include <cstdint>
#include <future>
#include <thread>

namespace {

constexpr const char* kHost = "127.0.0.1";

auto StartServer(std::uint32_t port) -> std::unique_ptr<qdb::core::TcpServer> {
    auto server = std::make_unique<qdb::core::TcpServer>(kHost, port);
    if (!server->Start()) {
        return nullptr;
    }
    return server;
}

}  // namespace

TEST(TcpServer, CanBeCreated) {
    auto server = qdb::core::TcpServer::New("127.0.0.1", 8080);

    EXPECT_NE(server, nullptr);
}

TEST(TcpServer, InitiallyNotRunning) {
    qdb::core::TcpServer server("127.0.0.1", 8080);

    EXPECT_FALSE(server.IsRunning());
}

TEST(TcpServer, HasCorrectHostAndPort) {
    qdb::core::TcpServer server("127.0.0.1", 8080);

    EXPECT_EQ(server.Host(), "127.0.0.1");
    EXPECT_EQ(server.Port(), 8080);
}

TEST(TcpServer, StartAndStop) {
    auto server = StartServer(55300);
    if (!server) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }
    EXPECT_TRUE(server->IsRunning());

    server->Stop();
    EXPECT_FALSE(server->IsRunning());
}

TEST(TcpServer, AcceptsConnection) {
    std::unique_ptr<qdb::core::TcpServer> server;
    std::uint32_t port = 0;

    for (std::uint32_t attempt = 0; attempt < 10; ++attempt) {
        std::uint32_t candidate = 55400 + attempt;
        server = StartServer(candidate);
        if (server) {
            port = candidate;
            break;
        }
    }

    if (!server) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }

    std::promise<bool> accepted;
    auto accepted_future = accepted.get_future();

    std::thread server_thread([srv = std::move(server), accepted_signal = std::move(accepted)]() mutable {
        auto client = srv->Accept();
        accepted_signal.set_value(client != nullptr);
        srv->Stop();
    });

    qdb::core::TcpClient client(kHost, port);
    ASSERT_TRUE(client.Connect());

    EXPECT_TRUE(accepted_future.get());
    server_thread.join();
}

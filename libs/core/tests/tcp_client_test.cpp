#include <gtest/gtest.h>

#include <qdb/core/tcp_client.h>
#include <qdb/core/tcp_server.h>

#include <nlohmann/json.hpp>

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

TEST(TcpClient, CanBeCreated) {
    auto client = qdb::core::TcpClient::New("127.0.0.1", 8080);

    EXPECT_NE(client, nullptr);
}

TEST(TcpClient, InitiallyNotConnected) {
    qdb::core::TcpClient client("127.0.0.1", 8080);

    EXPECT_FALSE(client.IsConnected());
}

TEST(TcpClient, HasCorrectHostAndPort) {
    qdb::core::TcpClient client("127.0.0.1", 8080);

    EXPECT_EQ(client.Host(), "127.0.0.1");
    EXPECT_EQ(client.Port(), 8080);
}

TEST(TcpClient, SendFailsWhenDisconnected) {
    qdb::core::TcpClient client("127.0.0.1", 8080);
    nlohmann::json request = {
        {"action", "echo"},
        {"data", {{"message", "ping"}}},
    };

    EXPECT_FALSE(client.Send(request));
}

TEST(TcpClient, ReceiveFailsWhenDisconnected) {
    qdb::core::TcpClient client("127.0.0.1", 8080);

    EXPECT_FALSE(client.Receive().has_value());
}

TEST(TcpClient, ConnectsAndDisconnects) {
    std::unique_ptr<qdb::core::TcpServer> server;
    std::uint32_t port = 0;

    for (std::uint32_t attempt = 0; attempt < 10; ++attempt) {
        std::uint32_t candidate = 55200 + attempt;
        server = StartServer(candidate);
        if (server) {
            port = candidate;
            break;
        }
    }

    ASSERT_NE(server, nullptr) << "No free port found for client test server";

    std::promise<void> accepted;
    auto accepted_future = accepted.get_future();

    std::thread server_thread([srv = std::move(server), accepted_signal = std::move(accepted)]() mutable {
        (void)srv->Accept();
        srv->Stop();
        accepted_signal.set_value();
    });

    qdb::core::TcpClient client(kHost, port);
    EXPECT_TRUE(client.Connect());
    EXPECT_TRUE(client.IsConnected());
    client.Disconnect();
    EXPECT_FALSE(client.IsConnected());

    accepted_future.wait();
    server_thread.join();
}

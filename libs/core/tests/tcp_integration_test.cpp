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

TEST(TcpIntegration, EchoResponse) {
    std::unique_ptr<qdb::core::TcpServer> server;
    std::uint32_t port = 0;

    for (std::uint32_t attempt = 0; attempt < 10; ++attempt) {
        std::uint32_t candidate = 55000 + attempt;
        server = StartServer(candidate);
        if (server) {
            port = candidate;
            break;
        }
    }

    if (!server) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }

    std::promise<void> done;
    auto done_future = done.get_future();

    std::thread server_thread([srv = std::move(server), done_signal = std::move(done)]() mutable {
        auto client = srv->Accept();
        if (client) {
            auto request = client->Receive();
            if (request) {
                nlohmann::json response = {
                    {"status", "success"},
                    {"echo", *request},
                };
                client->Send(response);
            }
        }

        srv->Stop();
        done_signal.set_value();
    });

    qdb::core::TcpClient client(kHost, port);
    ASSERT_TRUE(client.Connect());

    nlohmann::json request = {
        {"action", "echo"},
        {"data", {{"message", "ping"}}},
    };

    ASSERT_TRUE(client.Send(request));

    auto response = client.Receive();
    ASSERT_TRUE(response.has_value());

    EXPECT_EQ((*response)["status"], "success");
    EXPECT_EQ((*response)["echo"]["action"], "echo");
    EXPECT_EQ((*response)["echo"]["data"]["message"], "ping");

    done_future.wait();
    server_thread.join();
}

#include <gtest/gtest.h>

#include <qdb/core/socket.h>
#include <qdb/core/tcp_server.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

TEST(Socket, DefaultIsInvalid) {
    qdb::core::Socket socket;
    EXPECT_FALSE(socket.IsValid());
}

TEST(Socket, SendRecvOverSocketPair) {
    std::array<int, 2> fds = {-1, -1};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds.data()), 0);

    qdb::core::Socket sender(fds[0]);
    qdb::core::Socket receiver(fds[1]);

    std::array<std::uint8_t, 4> payload = {1, 2, 3, 4};
    std::array<std::uint8_t, 4> buffer = {0, 0, 0, 0};

    EXPECT_TRUE(sender.Send(payload.data(), sizeof(payload)));
    EXPECT_TRUE(receiver.Recv(buffer.data(), sizeof(buffer)));

    EXPECT_EQ(buffer[0], 1);
    EXPECT_EQ(buffer[1], 2);
    EXPECT_EQ(buffer[2], 3);
    EXPECT_EQ(buffer[3], 4);
}

TEST(Socket, ConnectAndDisconnect) {
    std::unique_ptr<qdb::core::TcpServer> server;
    std::uint32_t port = 0;

    for (std::uint32_t attempt = 0; attempt < 10; ++attempt) {
        std::uint32_t candidate = 55100 + attempt;
        server = StartServer(candidate);
        if (server) {
            port = candidate;
            break;
        }
    }

    ASSERT_NE(server, nullptr) << "No free port found for socket test server";

    std::promise<void> accepted;
    auto accepted_future = accepted.get_future();

    std::thread server_thread([srv = std::move(server), accepted_signal = std::move(accepted)]() mutable {
        (void)srv->Accept();
        srv->Stop();
        accepted_signal.set_value();
    });

    qdb::core::Socket socket;
    ASSERT_TRUE(socket.Connect(kHost, port));
    EXPECT_TRUE(socket.IsValid());
    socket.Disconnect();
    EXPECT_FALSE(socket.IsValid());

    accepted_future.wait();
    server_thread.join();
}
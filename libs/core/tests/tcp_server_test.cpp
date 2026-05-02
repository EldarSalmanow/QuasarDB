#include <gtest/gtest.h>

#include <qdb/core/tcp_server.h>

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

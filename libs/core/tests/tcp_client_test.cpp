#include <gtest/gtest.h>

#include <qdb/core/tcp_client.h>

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

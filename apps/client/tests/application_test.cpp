#include <qdb/client/application.h>
#include <qdb/core/response.h>
#include <qdb/core/tcp_server.h>

#include <gtest/gtest.h>

#include <thread>

#include "test_support.h"

namespace qdb::client::test {

TEST(ApplicationTest, ReportsErrorWhenFileIsMissing) {
    const auto port = FindFreePort();
    qdb::core::TcpServer server("127.0.0.1", port);
    ASSERT_TRUE(server.Start());

    std::thread server_thread([&server]() {
        auto client = server.Accept();
        if (client) {
            client->Disconnect();
        }
        server.Stop();
    });

    const auto config = Config::New("127.0.0.1", port, "missing_script.sql");
    auto application = Application::New(config);

    OutputCapture capture(std::cout);
    const auto result = application->Run();

    server.Stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }

    EXPECT_EQ(result, 1);
    EXPECT_TRUE(Contains(capture.Str(), "Failed to open file: missing_script.sql"));
}

TEST(ApplicationTest, ExecutesFileAndReceivesResponse) {
    ScopedTempFile file("SELECT * FROM users;\n");
    const auto port = FindFreePort();
    qdb::core::TcpServer server("127.0.0.1", port);
    ASSERT_TRUE(server.Start());

    std::atomic<bool> server_ok{true};

    std::thread server_thread([&server, &server_ok]() {
        auto client = server.Accept();
        if (!client) {
            server_ok.store(false);
            server.Stop();
            return;
        }

        auto request = client->ReceiveRequest();
        if (request) {
            server_ok.store(server_ok.load() && request->Action() == "query");
            server_ok.store(server_ok.load() && request->Query() == "SELECT * FROM users;");
        } else {
            server_ok.store(false);
            server.Stop();
            return;
        }

        auto response = qdb::core::ResponseBuilder::Success()
                            .Message("Query executed")
                            .Data(nlohmann::json::parse(R"([{"id":1,"name":"Alice"}])"))
                            .Build();

        server_ok.store(server_ok.load() && client->SendResponse(response));
        client->Disconnect();
        server.Stop();
    });

    const auto config = Config::New("127.0.0.1", port, file.Path().string());
    auto application = Application::New(config);

    OutputCapture capture(std::cout);
    const auto result = application->Run();

    server.Stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(server_ok.load());
    EXPECT_TRUE(Contains(capture.Str(), "Query executed"));
    EXPECT_TRUE(Contains(capture.Str(), "Alice"));
    EXPECT_TRUE(Contains(capture.Str(), "row(s) returned"));
}

TEST(ApplicationTest, ReportsConnectionFailureWhenServerIsUnavailable) {
    const auto port = FindFreePort();
    const auto config = Config::New("127.0.0.1", port, "");
    auto application = Application::New(config);

    OutputCapture capture(std::cout);
    const auto result = application->Run();

    EXPECT_EQ(result, 1);
    EXPECT_TRUE(Contains(capture.Str(), "Failed to connect to server 127.0.0.1:"));
}

}  // namespace qdb::client::test





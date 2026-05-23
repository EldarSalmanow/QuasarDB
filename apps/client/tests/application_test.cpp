#include <qdb/client/application.h>
#include <qdb/core/response.h>
#include <qdb/core/tcp_server.h>

#include <gtest/gtest.h>

#include <thread>

#include "test_support.h"

namespace qdb::client::test {

TEST(ApplicationTest, ReportsErrorWhenFileIsMissing) {
    const auto port = FindFreePort();
    if (port == 0) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }
    qdb::core::TcpServer server("127.0.0.1", port);
    if (!server.Start()) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }

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
    if (port == 0) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }
    qdb::core::TcpServer server("127.0.0.1", port);
    if (!server.Start()) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }

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
    if (port == 0) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }
    const auto config = Config::New("127.0.0.1", port, "");
    auto application = Application::New(config);

    OutputCapture capture(std::cout);
    const auto result = application->Run();

    EXPECT_EQ(result, 1);
    EXPECT_TRUE(Contains(capture.Str(), "Failed to connect to server 127.0.0.1:"));
}

TEST(ApplicationTest, AsyncQueryPollingCompletes) {
    ScopedTempFile file("SELECT COUNT(id) FROM users;\n");
    const auto port = FindFreePort();
    if (port == 0) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }
    qdb::core::TcpServer server("127.0.0.1", port);
    if (!server.Start()) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }

    std::atomic<int> poll_count{0};

    std::thread server_thread([&server, &poll_count]() {
        auto client = server.Accept();
        if (!client) {
            server.Stop();
            return;
        }

        auto request = client->ReceiveRequest();
        ASSERT_TRUE(request.has_value());
        ASSERT_EQ(request->Action(), "query");

        auto pending = qdb::core::ResponseBuilder::Pending()
                           .Message("Operation is running in background")
                           .Data({{"task_id", "550e8400-e29b-41d4-a716-446655440000"}})
                           .Build();
        ASSERT_TRUE(client->SendResponse(pending));

        for (int i = 0; i < 3; ++i) {
            request = client->ReceiveRequest();
            ASSERT_TRUE(request.has_value());
            ASSERT_EQ(request->Action(), "check_task");
            ++poll_count;

            auto still_pending = qdb::core::ResponseBuilder::Pending()
                                    .Message("Operation is still running")
                                    .Data({{"task_id", "550e8400-e29b-41d4-a716-446655440000"}})
                                    .Build();
            ASSERT_TRUE(client->SendResponse(still_pending));
        }

        request = client->ReceiveRequest();
        ASSERT_TRUE(request.has_value());
        ASSERT_EQ(request->Action(), "check_task");
        ++poll_count;

        auto completed = qdb::core::ResponseBuilder::Success()
                             .Message("Query executed")
                             .Data(nlohmann::json::parse(R"([{"count":5}])"))
                             .Build();
        ASSERT_TRUE(client->SendResponse(completed));

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
    EXPECT_GE(poll_count.load(), 4);
    EXPECT_TRUE(Contains(capture.Str(), "Async query submitted"));
    EXPECT_TRUE(Contains(capture.Str(), "550e8400-e29b-41d4-a716-446655440000"));
    EXPECT_TRUE(Contains(capture.Str(), "Query executed"));
    EXPECT_TRUE(Contains(capture.Str(), "count"));
}

TEST(ApplicationTest, AsyncQueryPollingError) {
    ScopedTempFile file("SELECT COUNT(id) FROM users;\n");
    const auto port = FindFreePort();
    if (port == 0) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }
    qdb::core::TcpServer server("127.0.0.1", port);
    if (!server.Start()) {
        GTEST_SKIP() << "Loopback TCP bind is unavailable";
    }

    std::thread server_thread([&server]() {
        auto client = server.Accept();
        if (!client) {
            server.Stop();
            return;
        }

        auto request = client->ReceiveRequest();
        ASSERT_TRUE(request.has_value());
        ASSERT_EQ(request->Action(), "query");

        auto pending = qdb::core::ResponseBuilder::Pending()
                           .Message("Operation is running in background")
                           .Data({{"task_id", "550e8400-e29b-41d4-a716-446655440001"}})
                           .Build();
        ASSERT_TRUE(client->SendResponse(pending));

        request = client->ReceiveRequest();
        ASSERT_TRUE(request.has_value());
        ASSERT_EQ(request->Action(), "check_task");

        auto error = qdb::core::ResponseBuilder::Error()
                         .Message("Task failed: timeout")
                         .Build();
        ASSERT_TRUE(client->SendResponse(error));

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
    EXPECT_TRUE(Contains(capture.Str(), "Async query submitted"));
    EXPECT_TRUE(Contains(capture.Str(), "Task failed: timeout"));
}

}  // namespace qdb::client::test

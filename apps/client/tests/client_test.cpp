#include <qdb/core/request.h>
#include <qdb/core/response.h>
#include <qdb/core/tcp_client.h>
#include <qdb/core/tcp_server.h>

#include <gtest/gtest.h>

#include <thread>

#include "test_support.h"

namespace qdb::client::test {

TEST(ClientTransportTest, TcpClientSendsRequestAndReceivesResponse) {
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
		if (!request) {
			server_ok.store(false);
			server.Stop();
			return;
		}

		server_ok.store(server_ok.load() && request->Method() == "ExecuteQuery");
		server_ok.store(server_ok.load() && request->Sql() == "SELECT * FROM users");
		server_ok.store(server_ok.load() && request->Token() == "token-123");

		auto response = qdb::core::ResponseBuilder::Ok()
							.Code("SUCCESS")
							.Message("Query executed")
							.Data(R"([{"id":1,"name":"Alice"}])")
							.Build();

		server_ok.store(server_ok.load() && client->SendResponse(response));
		client->Disconnect();
		server.Stop();
	});

	qdb::core::TcpClient client("127.0.0.1", port);
	ASSERT_TRUE(client.Connect());

	auto request = qdb::core::RequestBuilder::ExecuteQuery("SELECT * FROM users").Token("token-123").Build();
	ASSERT_TRUE(client.SendRequest(request));

	auto response = client.ReceiveResponse();
	ASSERT_TRUE(response.has_value());
	EXPECT_TRUE(response->IsSuccess());
	EXPECT_EQ(response->GetCode(), "SUCCESS");
	EXPECT_EQ(response->GetMessage(), "Query executed");
	EXPECT_EQ(response->GetData(), R"([{"id":1,"name":"Alice"}])");

	client.Disconnect();
	server.Stop();
	if (server_thread.joinable()) {
		server_thread.join();
	}

	EXPECT_TRUE(server_ok.load());
}

}  // namespace qdb::client::test

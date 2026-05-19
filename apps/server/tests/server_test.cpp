#include <qdb/server/application.h>
#include <qdb/server/router.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace qdb::server {

namespace {

auto TestConfig(const std::string& name) -> Config {
    const auto base = std::filesystem::temp_directory_path() / ("qdb_server_" + name);
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);

    return Config::New("127.0.0.1", 9000, false, "test-secret", (base / "accounts.json").string(),
                       (base / "rbac.json").string(), (base / "access.log").string());
}

}  // namespace

TEST(ServerPipelineTest, CreateTableCreatesStorageNodeAndRoutesTableQueries) {
    Application application(TestConfig("execute"));

    auto create_response = application.Process(
        qdb::core::RequestBuilder::Query("CREATE TABLE users (id INT NOT_NULL, name STRING DEFAULT \"anon\");")
            .Build());
    ASSERT_TRUE(create_response.IsSuccess());
    EXPECT_EQ(create_response.GetMessage(), "Storage node created");

    auto insert_response =
        application.Process(qdb::core::RequestBuilder::Query("INSERT INTO users (id) VALUE (1);").Build());
    ASSERT_TRUE(insert_response.IsSuccess());
    EXPECT_EQ(insert_response.GetMessage(), "Query routed");

    const auto data = nlohmann::json::parse(insert_response.GetData());
    EXPECT_EQ(data.at("table").get<std::string>(), "users");
}

TEST(ServerPipelineTest, QueryReturnsSyntaxErrors) {
    Application application(TestConfig("syntax"));

    auto response = application.Process(qdb::core::RequestBuilder::Query("SELECT @ FROM users;").Build());
    ASSERT_TRUE(response.IsError());
    EXPECT_EQ(response.GetMessage(), "Invalid token in SQL query");
}

TEST(ServerPipelineTest, UnknownActionsAreRejected) {
    Application application(TestConfig("async"));

    auto submit = application.Process(
        qdb::core::RequestBuilder{}.Action("unknown").Data({{"query", "SELECT * FROM users;"}}).Build());
    ASSERT_TRUE(submit.IsError());
    EXPECT_EQ(submit.GetMessage(), "Unsupported action: unknown");
}

TEST(RouterTest, MapsOneTableToOneStorageNodeAndRemovesItOnDrop) {
    Router router;

    auto users = router.CreateNode(TableRef("users"));
    auto orders = router.CreateNode(TableRef("orders"));

    EXPECT_NE(users.id, orders.id);
    EXPECT_EQ(router.Size(), 2);
    EXPECT_EQ(router.Resolve(TableRef("users"))->id, users.id);
    EXPECT_EQ(router.Resolve(TableRef("orders"))->id, orders.id);

    EXPECT_TRUE(router.DropNode(TableRef("users")));
    EXPECT_EQ(router.Size(), 1);
    EXPECT_FALSE(router.Resolve(TableRef("users")).has_value());
    EXPECT_TRUE(router.Resolve(TableRef("orders")).has_value());
}

}  // namespace qdb::server

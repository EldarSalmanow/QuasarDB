#include <qdb/server/application.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace qdb::server {

namespace {

auto TestConfig(const std::string& name) -> Config {
    const auto base = std::filesystem::temp_directory_path() / ("qdb_server_" + name);
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);

    return Config::New("127.0.0.1", 9000, false, "test-secret", (base / "accounts.json").string(),
                       (base / "rbac.json").string(), false);
}

}  // namespace

TEST(ServerPipelineTest, QueryToUnavailableStorageReturnsError) {
    Application application(TestConfig("execute"));

    auto create_response = application.Process(
        qdb::core::RequestBuilder::Query("CREATE TABLE users (id INT NOT_NULL, name STRING DEFAULT \"anon\");")
            .Build());
    ASSERT_TRUE(create_response.IsError());
    EXPECT_EQ(create_response.GetMessage(), "Failed to connect to storage node");
}

TEST(ServerPipelineTest, QueryReturnsSyntaxErrors) {
    Application application(TestConfig("syntax"));

    auto response = application.Process(qdb::core::RequestBuilder::Query("SELECT @ FROM users;").Build());
    ASSERT_TRUE(response.IsError());
    EXPECT_EQ(response.GetMessage(), "Invalid token in SQL query");
}

TEST(ServerPipelineTest, SemanticAnalyzerRejectsInvalidDefaultType) {
    Application application(TestConfig("semantic_default"));

    auto response =
        application.Process(qdb::core::RequestBuilder::Query("CREATE TABLE users (id INT DEFAULT \"abc\");").Build());
    ASSERT_TRUE(response.IsError());
    EXPECT_EQ(response.GetMessage(), "SEMANTIC_ERROR: column 'id' expects INT default");
}

TEST(ServerPipelineTest, LongQueryReturnsTaskIdImmediately) {
    Application application(TestConfig("async_query"));

    auto response = application.Process(qdb::core::RequestBuilder::Query("SELECT COUNT(id) FROM users;").Build());

    ASSERT_TRUE(response.IsPending());
    EXPECT_EQ(response.GetMessage(), "Operation is running in background");

    const auto& task_id = response.GetDataObject().at("task_id").get_ref<const std::string&>();
    EXPECT_EQ(task_id.size(), 36);
    EXPECT_EQ(task_id[14], '4');
    EXPECT_EQ(task_id[8], '-');
    EXPECT_EQ(task_id[13], '-');
    EXPECT_EQ(task_id[18], '-');
    EXPECT_EQ(task_id[23], '-');
}

TEST(ServerPipelineTest, UnknownActionsAreRejected) {
    Application application(TestConfig("async"));

    auto submit = application.Process(
        qdb::core::RequestBuilder{}.Action("unknown").Data({{"query", "SELECT * FROM users;"}}).Build());
    ASSERT_TRUE(submit.IsError());
    EXPECT_EQ(submit.GetMessage(), "Unsupported action: unknown");
}

}  // namespace qdb::server

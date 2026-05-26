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

auto AuthConfig(const std::string& name) -> Config {
    const auto base = std::filesystem::temp_directory_path() / ("qdb_server_auth_" + name);
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);

    return Config::New("127.0.0.1", 9000, true, "test-secret", (base / "accounts.json").string(),
                       (base / "rbac.json").string(), false);
}

}  // namespace

TEST(ServerPipelineTest, QueryToUnavailableStorageReturnsError) {
    Application application(TestConfig("execute"));

    auto db_response = application.Process(qdb::core::RequestBuilder::Query("CREATE DATABASE shop;").Build());
    ASSERT_TRUE(db_response.IsSuccess());

    auto create_response = application.Process(
        qdb::core::RequestBuilder::Query("CREATE TABLE shop.users (id INT NOT_NULL, name STRING DEFAULT \"anon\");")
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

    auto response = application.Process(qdb::core::RequestBuilder::Query("SELECT COUNT(id) FROM shop.users;").Build());

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

TEST(ServerPipelineTest, UseStoresDatabaseInSession) {
    Application application(TestConfig("use_session"));
    Session session;

    auto create_db = application.Process(qdb::core::RequestBuilder::Query("CREATE DATABASE shop;").Build(), session);
    ASSERT_TRUE(create_db.IsSuccess());

    auto use = application.Process(qdb::core::RequestBuilder::Query("USE shop;").Build(), session);
    ASSERT_TRUE(use.IsSuccess());

    auto create_table = application.Process(
        qdb::core::RequestBuilder::Query("CREATE TABLE users (id INT);").Build(), session);
    ASSERT_TRUE(create_table.IsError());
    EXPECT_EQ(create_table.GetMessage(), "Failed to connect to storage node");
}

TEST(ServerPipelineTest, UnqualifiedTableRequiresUse) {
    Application application(TestConfig("no_default"));

    auto response = application.Process(qdb::core::RequestBuilder::Query("CREATE TABLE users (id INT);").Build());

    ASSERT_TRUE(response.IsError());
    EXPECT_EQ(response.GetMessage(), "No database selected; run USE <database> or qualify the table as <database>.<table>");
}

TEST(ServerPipelineTest, UnknownActionsAreRejected) {
    Application application(TestConfig("async"));

    auto submit = application.Process(
        qdb::core::RequestBuilder{}.Action("unknown").Data({{"query", "SELECT * FROM users;"}}).Build());
    ASSERT_TRUE(submit.IsError());
    EXPECT_EQ(submit.GetMessage(), "Unsupported action: unknown");
}

TEST(ServerPipelineTest, AuthRequiresSuperuserSetupBeforeQueries) {
    Application application(AuthConfig("setup_required"));

    auto handshake = application.Process(qdb::core::RequestBuilder::Handshake().Build());
    ASSERT_TRUE(handshake.IsSuccess());
    EXPECT_TRUE(handshake.GetDataObject().value("auth_required", false));
    EXPECT_TRUE(handshake.GetDataObject().value("setup_required", false));

    auto response = application.Process(qdb::core::RequestBuilder::Query("CREATE DATABASE shop;").Build());

    ASSERT_TRUE(response.IsError());
    EXPECT_EQ(response.GetMessage(), "Superuser account setup required");
    EXPECT_TRUE(response.GetDataObject().value("setup_required", false));
}

TEST(ServerPipelineTest, FirstLoginCanCreateSuperuserAndToken) {
    Application application(AuthConfig("bootstrap"));

    auto setup = application.Process(qdb::core::RequestBuilder::CreateSuperuser("admin", "secret").Build());
    ASSERT_TRUE(setup.IsSuccess());
    EXPECT_EQ(setup.GetMessage(), "Superuser account created");

    const auto token = setup.GetDataObject().at("token").get<std::string>();
    ASSERT_FALSE(token.empty());

    auto without_token = application.Process(qdb::core::RequestBuilder::Query("CREATE DATABASE shop;").Build());
    ASSERT_TRUE(without_token.IsError());
    EXPECT_EQ(without_token.GetMessage(), "Valid token is required");

    auto with_token = application.Process(
        qdb::core::RequestBuilder::Query("CREATE DATABASE shop;").Token(token).Build());
    ASSERT_TRUE(with_token.IsSuccess());
}

TEST(ServerPipelineTest, LoginCreateIsOnlyForInitialSetup) {
    Application application(AuthConfig("bootstrap_once"));

    ASSERT_TRUE(application.Process(qdb::core::RequestBuilder::CreateSuperuser("admin", "secret").Build()).IsSuccess());

    auto response = application.Process(qdb::core::RequestBuilder::CreateSuperuser("root", "secret").Build());
    ASSERT_TRUE(response.IsError());
    EXPECT_EQ(response.GetMessage(), "Superuser setup is already complete");
}

}  // namespace qdb::server

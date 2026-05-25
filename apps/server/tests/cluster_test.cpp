#include <qdb/server/lexer.h>
#include <qdb/server/catalog.h>
#include <qdb/server/monitor.h>
#include <qdb/server/parser.h>
#include <qdb/server/router.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace qdb::server {

namespace {

auto Parse(const std::string& sql) -> std::unique_ptr<Statement> {
    Lexer lexer(sql);
    Parser parser(lexer.Tokenize());
    return parser.ParseStatement();
}

}  // namespace

TEST(MonitorTest, ProbeOnceMarksNodeDownAfterMisses) {
    auto registry = Registry::New(false);
    StorageId id{"shop.users"};
    ASSERT_TRUE(registry->CreateNode(id));

    Monitor monitor(registry, std::chrono::milliseconds(1), 3);

    monitor.ProbeOnce();
    monitor.ProbeOnce();
    auto node = registry->GetNode(id);
    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->state, StorageState::Unknown);

    monitor.ProbeOnce();
    node = registry->GetNode(id);
    ASSERT_TRUE(node.has_value());
    EXPECT_EQ(node->state, StorageState::Down);
}

TEST(RouterTest, ReturnsErrorForDeadStorageNode) {
    auto registry = Registry::New(false);
    StorageId id{"shop.users"};
    ASSERT_TRUE(registry->CreateNode(id));
    ASSERT_TRUE(registry->UpdateNode(id, StorageState::Down));

    const auto catalog_path = std::filesystem::temp_directory_path() / "qdb_router_catalog.json";
    std::filesystem::remove(catalog_path);
    Catalog catalog(catalog_path);
    ASSERT_TRUE(catalog.CreateDatabase("shop"));
    auto create = Parse("CREATE TABLE shop.users (id INT);");
    ASSERT_TRUE(catalog.CreateTable(*dynamic_cast<CreateTableStmt*>(create.get())));

    Router router(registry, catalog);
    auto statement = Parse("INSERT INTO shop.users (id) VALUE (1);");
    auto response = router.Route(*statement);

    ASSERT_TRUE(response.IsError());
    EXPECT_EQ(response.GetMessage(), "Storage node is down");
}

}  // namespace qdb::server

#include <qdb/server/router.h>

#include <qdb/core/request.h>
#include <qdb/core/tcp_client.h>
#include <qdb/server/analyzer.h>

#include <utility>

namespace qdb::server {

Router::Router(std::shared_ptr<Registry> registry, Catalog& catalog)
        : registry_(std::move(registry)),
          catalog_(catalog) {}

auto Router::New(std::shared_ptr<Registry> registry, Catalog& catalog) -> std::unique_ptr<Router> {
    return std::make_unique<Router>(std::move(registry), catalog);
}

auto Router::Route(const Statement& statement) -> qdb::core::Response {
    if (statement.KindOf() == Statement::Kind::CreateDatabase) {
        const auto& create_db = static_cast<const CreateDatabaseStmt&>(statement);
        if (!catalog_.CreateDatabase(create_db.DatabaseName)) {
            return qdb::core::Error("Database already exists: " + create_db.DatabaseName);
        }
        return qdb::core::Success("Database created");
    }

    if (statement.KindOf() == Statement::Kind::DropDatabase) {
        const auto& drop_db = static_cast<const DropDatabaseStmt&>(statement);
        for (const auto& table : catalog_.DropDatabase(drop_db.DatabaseName)) {
            const StorageId id{TableKey(table)};
            DropTableStmt drop_table(table);
            SendToStorage(id, drop_table);
            registry_->DropNode(id);
        }
        return qdb::core::Success("Database dropped");
    }

    if (statement.KindOf() == Statement::Kind::UseDatabase) {
        const auto& use_db = static_cast<const UseDatabaseStmt&>(statement);
        if (!catalog_.HasDatabase(use_db.DatabaseName)) {
            return qdb::core::Error("Database not found: " + use_db.DatabaseName);
        }
        return qdb::core::Success("Database selected");
    }

    if (statement.KindOf() == Statement::Kind::CreateTable) {
        const auto& create = static_cast<const CreateTableStmt&>(statement);
        StorageId id {TableKey(create.Table)};

        if (!catalog_.CreateTable(create)) {
            return qdb::core::Error("Table already exists or database not found");
        }

        if (!registry_->CreateNode(id)) {
            catalog_.DropTable(create.Table);
            return qdb::core::Error("Failed to create storage node in registry");
        }

        auto response = SendToStorage(id, statement);
        if (response.IsError()) {
            registry_->DropNode(id);
            catalog_.DropTable(create.Table);
        }
        return response;
    }

    if (statement.KindOf() == Statement::Kind::DropTable) {
        const auto& drop = static_cast<const DropTableStmt&>(statement);
        StorageId id {TableKey(drop.Table)};
        if (!catalog_.HasTable(drop.Table)) {
            return qdb::core::Error("Table not found: " + TableKey(drop.Table));
        }

        auto response = SendToStorage(id, statement);

        if (response.IsSuccess()) {
            catalog_.DropTable(drop.Table);
            registry_->DropNode(id);
        }

        return response;
    }

    auto table = Analyzer::TableFromStatement(statement);

    if (!table.has_value()) {
        return qdb::core::Error("Cannot determine table from statement");
    }

    StorageId id {TableKey(table.value())};
    if (!catalog_.HasTable(table.value())) {
        return qdb::core::Error("Table not found: " + TableKey(table.value()));
    }

    return SendToStorage(id, statement);
}

auto Router::SendToStorage(const StorageId& id, const Statement& statement) -> qdb::core::Response {
    auto node = registry_->GetNode(id);

    if (!node.has_value()) {
        return qdb::core::Error("Storage node is not registered",
            {{"storage_id", id.table}});
    }

    if (node->state == StorageState::Down) {
        return qdb::core::Error("Storage node is down",
            {{"storage_id", id.table}, {"host", node->host}, {"port", node->port}});
    }

    auto client = qdb::core::TcpClient::New(node->host, node->port);

    if (!client || !client->Connect()) {
        return qdb::core::Error("Failed to connect to storage node",
                     {{"storage_id", id.table}, {"host", node->host}, {"port", node->port}});
    }

    const auto request = qdb::core::RequestBuilder()
        .Action("execute_ast")
        .Data({{"ast_root", SerializeAst(statement)}})
        .Build();

    if (!client->SendRequest(request)) {
        return qdb::core::Error("Failed to send request to storage", {{"storage_id", id.table}});
    }

    auto response = client->ReceiveResponse();

    client->Disconnect();

    if (!response.has_value()) {
        return qdb::core::Error("Failed to receive response from storage", {{"storage_id", id.table}});
    }

    return response.value();
}

auto Router::TableKey(const TableRef& table) -> std::string {
    return table.Database.empty() ? table.Table : table.Database + "." + table.Table;
}

}  // namespace qdb::server

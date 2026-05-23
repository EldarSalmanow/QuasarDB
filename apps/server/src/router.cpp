#include <qdb/server/router.h>

#include <qdb/core/tcp_client.h>
#include <qdb/server/analyzer.h>

#include <utility>

namespace qdb::server {

namespace {

auto Error(std::string message, nlohmann::json data = nlohmann::json::object()) -> qdb::core::Response {
    return qdb::core::ResponseBuilder::Error()
        .Message(std::move(message))
        .Data(std::move(data))
        .Build();
}

}  // namespace

Router::Router(std::shared_ptr<Registry> registry)
        : registry_(std::move(registry)) {}

auto Router::New(std::shared_ptr<Registry> registry) -> std::unique_ptr<Router> {
    return std::make_unique<Router>(std::move(registry));
}

auto Router::Route(const Statement& statement) -> qdb::core::Response {
    if (const auto* create = dynamic_cast<const CreateTableStmt*>(&statement)) {
        StorageId id {TableKey(create->Table)};

        if (!registry_->CreateNode(id)) {
            return Error("Failed to create storage node in registry");
        }

        return SendToStorage(id, statement);
    }

    if (const auto* drop = dynamic_cast<const DropTableStmt*>(&statement)) {
        StorageId id {TableKey(drop->Table)};

        auto response = SendToStorage(id, statement);

        if (response.IsSuccess()) {
            registry_->DropNode(id);
        }

        return response;
    }

    auto table = Analyzer::TableFromStatement(statement);

    if (!table.has_value()) {
        return Error("Cannot determine table from statement");
    }

    StorageId id {TableKey(table.value())};

    return SendToStorage(id, statement);
}

auto Router::SendToStorage(const StorageId& id, const Statement& statement) -> qdb::core::Response {
    auto node = registry_->GetNode(id);

    if (!node.has_value()) {
        return Error("Storage node is not registered",
            {{"storage_id", id.table}});
    }

    if (node->state == StorageState::Down) {
        return Error("Storage node is down",
            {{"storage_id", id.table}, {"host", node->host}, {"port", node->port}});
    }

    auto client = qdb::core::TcpClient::New(node->host, node->port);

    if (!client || !client->Connect()) {
        return Error("Failed to connect to storage node",
                     {{"storage_id", id.table}, {"host", node->host}, {"port", node->port}});
    }

    const nlohmann::json request = {
        {"action", "execute_ast"},
        {"data", {{"ast_root", SerializeAst(statement)}}}
    };

    if (!client->Send(request)) {
        return Error("Failed to send request to storage", {{"storage_id", id.table}});
    }

    auto response = client->Receive();

    client->Disconnect();

    if (!response.has_value()) {
        return Error("Failed to receive response from storage", {{"storage_id", id.table}});
    }

    return qdb::core::Response::FromJsonObject(response.value());
}

auto Router::TableKey(const TableRef& table) -> std::string {
    return table.Database.empty() ? table.Table : table.Database + "." + table.Table;
}

}  // namespace qdb::server

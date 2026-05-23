#include <qdb/server/router.h>
#include <qdb/server/analyzer.h>

#include <algorithm>
#include <utility>

namespace qdb::server {

Router::Router(std::shared_ptr<Registry> registry)
        : registry_(std::move(registry)) {}

auto Router::Route(const Statement& statement) -> qdb::core::Response {
    if (const auto* create = dynamic_cast<const CreateTableStmt*>(&statement)) {
        StorageId id { create->Table };

        if (!registry_->CreateNode(id)) {
            return qdb::core::ResponseBuilder::Error()
                .Message("[ERROR in qdb::server::Router] Can`t create node in registry!")
                .Build();
        }

        return SendToNode(id, statement);
    }

    if (const auto *drop = dynamic_cast<const DropTableStmt *>(&statement)) {
        StorageId id { drop->Table };

        if (!registry_->HasNode(id)) {
            return qdb::core::ResponseBuilder::Error()
                .Message("[ERROR in qdb::server::Router] Can`t find node in registry!")
                .Build();
        }

        auto response = SendToNode(id, statement);

        if (!registry_->DropNode(id)) {
            return qdb::core::ResponseBuilder::Error()
                .Message("[ERROR in qdb::server::Router] Can`t drop node in registry!")
                .Build();
        }

        return response;
    }

    auto table = Analyzer::TableFromStatement(statement);

    if (!table.has_value()) {
        return qdb::core::ResponseBuilder::Error()
            .Message("Cannot determine table from statement")
            .Build();
    }

    StorageId id { table };

    return SendToStorage(id, statement);
}

auto Router::SendToStorage(const StorageId &id, const Statement &statement) -> qdb::core::Response {
    if (!registry_->HasNode(id)) {
        return qdb::core::ResponseBuilder::Error()
            .Message("Failed to connect to storage node")
            .Build();
    }

    if (!node.client) {
        node.client = qdb::core::TcpClient::New(node.host, node.port);
    }

    if (!node.client->IsConnected() && !node.client->Connect()) {
        node.alive = false;
        return qdb::core::ResponseBuilder::Error().Message("Failed to connect to storage node").Build();
    }

    nlohmann::json request = {
        {"action", "execute_ast"},
        {"data", {
            {"ast_root", SerializeAst(statement)}
        }}
    };

    if (!node.client->Send(request)) {
        node.alive = false;
        return qdb::core::ResponseBuilder::Error().Message("Failed to send request to storage").Build();
    }

    auto response = node.client->Receive();

    if (!response.has_value()) {
        node.alive = false;
        return qdb::core::ResponseBuilder::Error().Message("Failed to receive response from storage").Build();
    }

    return qdb::core::Response::FromJsonObject(response.value());
}

auto Router::TableKey(const TableRef& table) -> std::string {
    return table.Database.empty() ? table.Table : table.Database + "." + table.Table;
}

}  // namespace qdb::server

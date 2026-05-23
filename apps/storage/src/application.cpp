#include <qdb/storage/application.h>

#include <filesystem>
#include <iostream>
#include <qdb/core/response.h>
#include <qdb/storage/executor.h>
#include <qdb/server/ast.h>

namespace qdb::storage {

namespace {

auto ToSchema(const std::vector<qdb::server::ColumnDef>& source) -> Schema {
    std::vector<Column> columns;
    columns.reserve(source.size());
    for (const auto& column : source) {
        auto flags = static_cast<std::uint8_t>((column.NotNull ? Column::NOT_NULL_FLAG : 0) |
                                               (column.Indexed ? Column::INDEXED_FLAG : 0));
        auto type = column.ColumnType == qdb::server::ColumnDef::Type::Int ? Column::INT : Column::STRING;
        if (!column.DefaultValue) {
            columns.emplace_back(column.Name, type, flags);
            continue;
        }

        auto default_type = Column::DefaultType::NULL_VALUE;
        int default_int = 0;
        std::string default_string;
        if (column.DefaultValue->LiteralType == qdb::server::Literal::Type::Integer) {
            default_type = Column::DefaultType::INT;
            default_int = std::stoi(column.DefaultValue->Value);
        } else if (column.DefaultValue->LiteralType == qdb::server::Literal::Type::String) {
            default_type = Column::DefaultType::STRING;
            default_string = column.DefaultValue->Value;
        }
        columns.emplace_back(column.Name, type, flags, default_type, default_int, std::move(default_string));
    }
    return Schema(std::move(columns));
}

}  // namespace

Application::Application(Config config)
    : config_(std::move(config)),
      server_(qdb::core::TcpServer::New(config_.Host(), config_.Port())) {
    OpenExistingTable();
}

auto Application::New(Config config) -> std::unique_ptr<Application> {
    return std::make_unique<Application>(std::move(config));
}

auto Application::Run() -> std::int32_t {
    if (!server_ || !server_->Start()) {
        std::cerr << "Failed to start storage on " << config_.Host() << ":" << config_.Port() << std::endl;
        return 1;
    }

    std::cout << "Storage listening on " << config_.Host() << ":" << config_.Port() << std::endl;

    while (server_->IsRunning()) {
        auto client = AcceptConnection();
        if (!client) {
            continue;
        }

        connection_ = std::move(client);
        while (true) {
            auto request = connection_->Receive();
            if (!request.has_value()) {
                break;
            }

            auto response = ProcessRequest(request.value());
            if (!response.has_value()) {
                break;
            }

            if (!connection_->Send(response.value())) {
                break;
            }
        }
    }

    return 0;
}

auto Application::AcceptConnection() -> std::unique_ptr<qdb::core::TcpClient> {
    if (!server_) {
        return nullptr;
    }

    return server_->Accept();
}

auto Application::ProcessRequest(const nlohmann::json& request) -> std::optional<nlohmann::json> {
    if (!request.is_object()) {
        return qdb::core::ResponseBuilder::Error()
            .Message("Request must be a JSON object")
            .Build()
            .ToJsonObject();
    }

    const std::string action = request.value("action", "");
    if (action.empty()) {
        return qdb::core::ResponseBuilder::Error().Message("Missing action").Build().ToJsonObject();
    }

    if (action == "execute_ast") {
        if (!request.contains("data") || !request["data"].contains("ast_root")) {
            return qdb::core::ResponseBuilder::Error().Message("Missing ast_root").Build().ToJsonObject();
        }

        return ExecuteAst(request["data"]["ast_root"]);
    }

    if (action == "ping") {
        return qdb::core::ResponseBuilder::Success().Message("pong").Build().ToJsonObject();
    }

    return qdb::core::ResponseBuilder::Error().Message("Unknown action").Build().ToJsonObject();
}

auto Application::ExecuteAst(const nlohmann::json& ast) -> std::optional<nlohmann::json> {
    try {
        auto stmt = qdb::server::DeserializeAst(ast);
        if (!stmt) {
            return qdb::core::ResponseBuilder::Error()
                .Message("Invalid or unsupported AST")
                .Build()
                .ToJsonObject();
        }

        if (const auto* create = dynamic_cast<const qdb::server::CreateTableStmt*>(stmt.get())) {
            return CreateTable(*create);
        }
        if (const auto* drop = dynamic_cast<const qdb::server::DropTableStmt*>(stmt.get())) {
            return DropTable(*drop);
        }
        if (dynamic_cast<const qdb::server::CreateDatabaseStmt*>(stmt.get()) != nullptr ||
            dynamic_cast<const qdb::server::DropDatabaseStmt*>(stmt.get()) != nullptr ||
            dynamic_cast<const qdb::server::UseDatabaseStmt*>(stmt.get()) != nullptr) {
            return qdb::core::ResponseBuilder::Error()
                .Message("Storage shard does not manage databases")
                .Build()
                .ToJsonObject();
        }
        if (!table_) {
            return qdb::core::ResponseBuilder::Error().Message("Table shard is not initialized").Build().ToJsonObject();
        }

        Executor exec(*table_, interner_);
        stmt->Accept(exec);
        return exec.Result();
    } catch (const std::exception& ex) {
        return qdb::core::ResponseBuilder::Error().Message(ex.what()).Build().ToJsonObject();
    }
}

auto Application::CreateTable(const qdb::server::CreateTableStmt& statement) -> nlohmann::json {
    if (table_) {
        return qdb::core::ResponseBuilder::Error().Message("Table shard is already initialized").Build().ToJsonObject();
    }

    std::filesystem::create_directories(config_.Root());
    table_ = std::make_unique<Table>(statement.Table.Table, config_.Root(), ToSchema(statement.Columns), &interner_);
    return qdb::core::ResponseBuilder::Success()
        .Message("Table shard created")
        .Data({{"rows_affected", 0}})
        .Build()
        .ToJsonObject();
}

auto Application::DropTable(const qdb::server::DropTableStmt& statement) -> nlohmann::json {
    if (!table_ || table_->name() != statement.Table.Table) {
        return qdb::core::ResponseBuilder::Error().Message("Table not found: " + statement.Table.Table).Build().ToJsonObject();
    }

    table_->drop();
    table_.reset();
    return qdb::core::ResponseBuilder::Success()
        .Message("Table shard dropped")
        .Data({{"rows_affected", 0}})
        .Build()
        .ToJsonObject();
}

auto Application::OpenExistingTable() -> void {
    const auto root = std::filesystem::path(config_.Root());
    if (!std::filesystem::exists(root)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().extension() == ".schema") {
            table_ = std::make_unique<Table>(entry.path().stem().string(), root, &interner_);
            return;
        }
    }
}

}  // namespace qdb::storage

#include <qdb/storage/application.h>

#include <filesystem>
#include <qdb/storage/executor.h>
#include <qdb/server/ast.h>

namespace qdb::storage {

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
        return 1;
    }

    while (server_->IsRunning()) {
        auto client = AcceptConnection();
        if (!client) {
            continue;
        }

        connection_ = std::move(client);
        while (true) {
            auto request = connection_->ReceiveRequest();
            if (!request.has_value()) {
                break;
            }

            if (!connection_->SendResponse(ProcessRequest(request.value()))) {
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

auto Application::ProcessRequest(const qdb::core::Request& request) -> qdb::core::Response {
    if (request.Action() == "ping") {
        return qdb::core::Success("pong");
    }
    if (request.Action() != "execute_ast") {
        return qdb::core::Error("Unknown action");
    }
    if (!request.Data().contains("ast_root")) {
        return qdb::core::Error("Missing ast_root");
    }
    return ExecuteAst(request.Data()["ast_root"]);
}

auto Application::ExecuteAst(const nlohmann::json& ast) -> qdb::core::Response {
    try {
        auto stmt = qdb::server::DeserializeAst(ast);
        if (!stmt) {
            return qdb::core::Error("Invalid or unsupported AST");
        }

        if (stmt->KindOf() == qdb::server::Statement::Kind::CreateTable) {
            return CreateTable(static_cast<const qdb::server::CreateTableStmt&>(*stmt));
        }
        if (stmt->KindOf() == qdb::server::Statement::Kind::DropTable) {
            return DropTable(static_cast<const qdb::server::DropTableStmt&>(*stmt));
        }
        if (stmt->KindOf() == qdb::server::Statement::Kind::CreateDatabase ||
            stmt->KindOf() == qdb::server::Statement::Kind::DropDatabase ||
            stmt->KindOf() == qdb::server::Statement::Kind::UseDatabase) {
            return qdb::core::Error("Storage shard does not manage databases");
        }
        if (!table_) {
            return qdb::core::Error("Table shard is not initialized");
        }

        Executor exec(*table_, interner_);
        return qdb::core::Response::FromJsonObject(exec.Execute(*stmt));
    } catch (const std::exception& ex) {
        return qdb::core::Error(ex.what());
    }
}

auto Application::CreateTable(const qdb::server::CreateTableStmt& statement) -> qdb::core::Response {
    if (table_) {
        return qdb::core::Error("Table shard is already initialized");
    }

    std::filesystem::create_directories(config_.Root());
    table_ = std::make_unique<Table>(statement.Table.Table, config_.Root(), ToSchema(statement.Columns), &interner_);
    return qdb::core::Success("Table shard created", {{"rows_affected", 0}});
}

auto Application::DropTable(const qdb::server::DropTableStmt& statement) -> qdb::core::Response {
    if (!table_ || table_->name() != statement.Table.Table) {
        return qdb::core::Error("Table not found: " + statement.Table.Table);
    }

    table_->drop();
    table_.reset();
    return qdb::core::Success("Table shard dropped", {{"rows_affected", 0}});
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

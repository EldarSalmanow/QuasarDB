#ifndef QUASARDB_SERVER_CONFIG_H
#define QUASARDB_SERVER_CONFIG_H

#include <qdb/server/router.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace qdb::server {

class Config {
public:
    Config(std::string host, std::uint32_t port, bool auth_required, std::string jwt_secret,
           std::string account_path, std::string rbac_path, std::string access_log_path,
           std::vector<StorageNode> storage_nodes = {});

public:
    static auto New(std::string host, std::uint32_t port, bool auth_required = false,
                    std::string jwt_secret = "quasardb-dev-secret",
                    std::string account_path = "qdb_accounts.json",
                    std::string rbac_path = "qdb_rbac.json",
                    std::string access_log_path = "qdb_access.log",
                    std::vector<StorageNode> storage_nodes = {}) -> Config;

    static auto FromArguments(int argc, char** argv) -> std::optional<Config>;

public:
    auto Host() const -> const std::string&;

    auto Port() const -> std::uint32_t;

    auto AuthRequired() const -> bool;

    auto JwtSecret() const -> const std::string&;

    auto AccountPath() const -> const std::string&;

    auto RbacPath() const -> const std::string&;

    auto AccessLogPath() const -> const std::string&;

    auto StorageNodes() const -> const std::vector<StorageNode>&;

private:
    std::string host_;

    std::uint32_t port_;

    bool auth_required_;

    std::string jwt_secret_;

    std::string account_path_;

    std::string rbac_path_;

    std::string access_log_path_;

    std::vector<StorageNode> storage_nodes_;
};

}  // namespace qdb::server

#endif  // QUASARDB_SERVER_CONFIG_H

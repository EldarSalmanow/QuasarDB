#ifndef QUASARDB_SERVER_CONFIG_H
#define QUASARDB_SERVER_CONFIG_H

#include <cstdint>
#include <optional>
#include <string>

namespace qdb::server {

class Config {
public:
    Config(std::string host, std::uint32_t port, bool auth_required, std::string jwt_secret,
           std::string account_path, std::string rbac_path, bool auto_start_storage,
           std::string storage_binary, std::string storage_root);

public:
    static auto New(std::string host, std::uint32_t port, bool auth_required = false,
                    std::string jwt_secret = "quasardb-dev-secret",
                    std::string account_path = "qdb_accounts.json",
                    std::string rbac_path = "qdb_rbac.json",
                    bool auto_start_storage = true,
                    std::string storage_binary = "",
                    std::string storage_root = "data/storage") -> Config;

    static auto FromArguments(int argc, char** argv) -> std::optional<Config>;

public:
    auto Host() const -> const std::string&;

    auto Port() const -> std::uint32_t;

    auto AuthRequired() const -> bool;

    auto JwtSecret() const -> const std::string&;

    auto AccountPath() const -> const std::string&;

    auto RbacPath() const -> const std::string&;

    auto AutoStartStorage() const -> bool;

    auto StorageBinary() const -> const std::string&;

    auto StorageRoot() const -> const std::string&;

private:
    std::string host_;

    std::uint32_t port_;

    bool auth_required_;

    std::string jwt_secret_;

    std::string account_path_;

    std::string rbac_path_;

    bool auto_start_storage_;

    std::string storage_binary_;

    std::string storage_root_;
};

}  // namespace qdb::server

#endif  // QUASARDB_SERVER_CONFIG_H

#ifndef QUASARDB_RBAC_H
#define QUASARDB_RBAC_H

#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <vector>

namespace qdb::server {

enum class Permission {
    READ,
    WRITE,
    CREATE,
    DELETE
};

NLOHMANN_JSON_SERIALIZE_ENUM(Permission, {
    {Permission::READ, "READ"},
    {Permission::WRITE, "WRITE"},
    {Permission::CREATE, "CREATE"},
    {Permission::DELETE, "DELETE"},
})

struct AccessRule {
    std::string user_id;
    std::string database;
    std::string table;
    std::set<Permission> permissions;
};

void to_json(nlohmann::json& j, const AccessRule& r);
void from_json(const nlohmann::json& j, AccessRule& r);

class RBACManager {
public:
    explicit RBACManager(std::string storage_path);

    auto CheckPermission(const std::string& username, const std::string& database,
                         const std::string& table, Permission perm) const -> bool;

    auto GrantPermission(const std::string& username, const std::string& database,
                         const std::string& table, Permission perm) -> void;

    auto RevokePermission(const std::string& username, const std::string& database,
                          const std::string& table, Permission perm) -> void;

    auto GetUserPermissions(const std::string& username) const -> std::vector<AccessRule>;

    auto GetAllRules() const -> const std::vector<AccessRule>&;

private:
    void Load();
    void Save() const;

    std::string storage_path_;
    std::vector<AccessRule> rules_;
};

}  // namespace qdb::server

#endif  // QUASARDB_RBAC_H

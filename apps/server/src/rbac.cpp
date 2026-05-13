#include <qdb/server/rbac.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace qdb::server {

void to_json(nlohmann::json& j, const AccessRule& r) {
    j = nlohmann::json{
        {"user_id", r.user_id},
        {"database", r.database},
        {"table", r.table},
        {"permissions", r.permissions}
    };
}

void from_json(const nlohmann::json& j, AccessRule& r) {
    j.at("user_id").get_to(r.user_id);
    j.at("database").get_to(r.database);
    j.at("table").get_to(r.table);
    j.at("permissions").get_to(r.permissions);
}

RBACManager::RBACManager(std::string storage_path) : storage_path_(std::move(storage_path)) {
    Load();
}

auto RBACManager::CheckPermission(const std::string& username, const std::string& database,
                                  const std::string& table, Permission perm) const -> bool {
    auto matches = [&](const AccessRule& rule) -> bool {
        if (rule.user_id != username && rule.user_id != "*") return false;
        if (rule.database != database && rule.database != "*") return false;
        if (rule.table != table && rule.table != "*") return false;
        return rule.permissions.find(perm) != rule.permissions.end();
    };

    auto it = std::find_if(rules_.begin(), rules_.end(), matches);
    return it != rules_.end();
}

auto RBACManager::GrantPermission(const std::string& username, const std::string& database,
                                  const std::string& table, Permission perm) -> void {
    if (perm == Permission::INVALID) return;
    auto it = std::find_if(rules_.begin(), rules_.end(), [&](const AccessRule& r) {
        return r.user_id == username && r.database == database && r.table == table;
    });

    if (it != rules_.end()) {
        it->permissions.insert(perm);
    } else {
        rules_.push_back({username, database, table, {perm}});
    }

    if (!Save()) {
        if (it != rules_.end()) {
            it->permissions.erase(perm);
        } else {
            rules_.pop_back();
        }
    }
}

auto RBACManager::RevokePermission(const std::string& username, const std::string& database,
                                   const std::string& table, Permission perm) -> void {
    auto it = std::find_if(rules_.begin(), rules_.end(), [&](const AccessRule& r) {
        return r.user_id == username && r.database == database && r.table == table;
    });

    if (it == rules_.end()) return;

    auto old_perms = it->permissions;
    it->permissions.erase(perm);
    bool rule_erased = false;
    AccessRule erased_rule;
    size_t erased_pos = 0;
    if (it->permissions.empty()) {
        erased_rule = *it;
        erased_pos = static_cast<size_t>(it - rules_.begin());
        rules_.erase(it);
        rule_erased = true;
    }

    if (!Save()) {
        if (rule_erased) {
            rules_.insert(rules_.begin() + static_cast<ptrdiff_t>(erased_pos), erased_rule);
        } else {
            for (auto& r : rules_) {
                if (r.user_id == username && r.database == database && r.table == table) {
                    r.permissions = old_perms;
                    break;
                }
            }
        }
    }
}

auto RBACManager::GetUserPermissions(const std::string& username) const -> std::vector<AccessRule> {
    std::vector<AccessRule> result;
    for (const auto& rule : rules_) {
        if (rule.user_id == username || rule.user_id == "*") {
            result.push_back(rule);
        }
    }
    return result;
}

auto RBACManager::GetAllRules() const -> const std::vector<AccessRule>& {
    return rules_;
}

void RBACManager::Load() {
    std::ifstream file(storage_path_);
    if (!file.is_open()) return;

    try {
        nlohmann::json j;
        file >> j;
        rules_ = j.get<std::vector<AccessRule>>();
    } catch (...) {
    }
}

auto RBACManager::Save() const -> bool {
    auto tmp_path = storage_path_ + ".tmp";
    nlohmann::json j = rules_;
    std::ofstream file(tmp_path, std::ios::trunc);
    if (!file.is_open()) return false;
    std::error_code ec;
    std::filesystem::permissions(tmp_path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace, ec);
    if (ec) { file.close(); std::filesystem::remove(tmp_path); return false; }
    file << j.dump(4);
    if (!file.good()) { file.close(); std::filesystem::remove(tmp_path); return false; }
    file.close();
    if (file.fail()) { std::filesystem::remove(tmp_path); return false; }
    std::filesystem::rename(tmp_path, storage_path_, ec);
    if (ec) { std::filesystem::remove(tmp_path); return false; }
    return true;
}

}  // namespace qdb::server

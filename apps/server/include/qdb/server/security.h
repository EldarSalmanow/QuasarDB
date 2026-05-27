#ifndef QUASARDB_SECURITY_H
#define QUASARDB_SECURITY_H

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace qdb::server {

struct Account {
    std::string username;
    std::string password_hash;
    std::string salt;
};

void to_json(nlohmann::json& j, const Account& a);
void from_json(const nlohmann::json& j, Account& a);

class AccountStore {
public:
    explicit AccountStore(std::string storage_path);

    auto CreateAccount(const std::string& username, const std::string& password) -> bool;
    auto Authenticate(const std::string& username, const std::string& password) const -> bool;
    auto HasAccount(const std::string& username) const -> bool;
    auto Empty() const -> bool;

private:
    auto Load() -> bool;
    auto Save() -> bool;

    std::string storage_path_;
    std::unordered_map<std::string, Account> accounts_;
    bool load_ok_{false};
};

auto ComputeSha256(const std::vector<std::uint8_t>& data) -> std::vector<std::uint8_t>;
auto ComputeHmacSha256(const std::vector<std::uint8_t>& key, const std::vector<std::uint8_t>& data) -> std::vector<std::uint8_t>;
auto GenerateSalt(size_t byte_length = 16) -> std::string;
auto HashPassword(const std::string& password, const std::string& salt) -> std::string;
auto Base64UrlEncode(const std::vector<std::uint8_t>& data) -> std::string;
auto Base64UrlDecode(const std::string& input) -> std::vector<std::uint8_t>;

class JwtHandler {
public:
    explicit JwtHandler(std::string secret_key);

    auto GenerateToken(const std::string& username, std::chrono::seconds ttl = std::chrono::hours(24)) const -> std::string;
    auto ValidateToken(const std::string& token) const -> std::optional<std::string>;

private:
    std::string secret_key_;
};

}  // namespace qdb::server

#endif  // QUASARDB_SECURITY_H

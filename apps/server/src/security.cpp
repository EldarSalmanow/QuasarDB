#include <qdb/server/security.h>

#include <jwt-cpp/traits/nlohmann-json/defaults.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace qdb::server {

void to_json(nlohmann::json& j, const Account& a) {
    j = nlohmann::json{{"username", a.username}, {"password_hash", a.password_hash}, {"salt", a.salt}};
}

void from_json(const nlohmann::json& j, Account& a) {
    j.at("username").get_to(a.username);
    j.at("password_hash").get_to(a.password_hash);
    j.at("salt").get_to(a.salt);
}

AccountStore::AccountStore(std::string storage_path) : storage_path_(std::move(storage_path)) {
    Load();
}

auto AccountStore::CreateAccount(const std::string& username, const std::string& password) -> bool {
    if (username.empty() || password.empty() || username == "*") return false;
    if (accounts_.find(username) != accounts_.end()) return false;

    Account account;
    account.username = username;
    account.salt = GenerateSalt();
    if (account.salt.empty()) return false;

    account.password_hash = HashPassword(password, account.salt);
    if (account.password_hash.empty()) return false;

    accounts_[username] = std::move(account);
    if (!Save()) {
        accounts_.erase(username);
        return false;
    }
    return true;
}

auto AccountStore::Authenticate(const std::string& username, const std::string& password) const -> bool {
    auto it = accounts_.find(username);
    if (it == accounts_.end()) {
        return false;
    }

    auto computed = HashPassword(password, it->second.salt);
    if (computed.size() != it->second.password_hash.size()) {
        return false;
    }

    return CRYPTO_memcmp(computed.data(), it->second.password_hash.data(), computed.size()) == 0;
}

auto AccountStore::HasAccount(const std::string& username) const -> bool {
    return accounts_.find(username) != accounts_.end();
}

auto AccountStore::Load() -> bool {
    std::ifstream file(storage_path_);
    if (!file.is_open()) {
        load_ok_ = true;
        return true;
    }

    try {
        nlohmann::json j;
        file >> j;
        accounts_ = j.get<std::unordered_map<std::string, Account>>();
        load_ok_ = true;
        return true;
    } catch (...) {
        load_ok_ = false;
        return false;
    }
}

auto AccountStore::Save() -> bool {
    if (!load_ok_) return false;
    auto tmp_path = storage_path_ + ".tmp";
    nlohmann::json j = accounts_;
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

auto ComputeSha256(const std::vector<std::uint8_t>& data) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> result(EVP_MAX_MD_SIZE);
    unsigned int len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return {};

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1
        || EVP_DigestUpdate(ctx, data.data(), data.size()) != 1
        || EVP_DigestFinal_ex(ctx, result.data(), &len) != 1) {
        EVP_MD_CTX_free(ctx);
        return {};
    }

    EVP_MD_CTX_free(ctx);
    result.resize(len);
    return result;
}

auto ComputeHmacSha256(const std::vector<std::uint8_t>& key, const std::vector<std::uint8_t>& data) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> result(EVP_MAX_MD_SIZE);
    unsigned int len = 0;

    auto* hmac_result = HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         data.data(), data.size(), result.data(), &len);
    if (hmac_result == nullptr) return {};

    result.resize(len);
    return result;
}

auto GenerateSalt(size_t byte_length) -> std::string {
    std::vector<unsigned char> bytes(byte_length);
    if (RAND_bytes(bytes.data(), static_cast<int>(byte_length)) != 1) {
        return {};
    }

    std::ostringstream oss;
    for (auto byte : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

auto HashPassword(const std::string& password, const std::string& salt) -> std::string {
    std::vector<unsigned char> hash(32);
    int rc = PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()),
                                reinterpret_cast<const unsigned char*>(salt.data()),
                                static_cast<int>(salt.size()), 600000,
                                EVP_sha256(), 32, hash.data());
    if (rc != 1) return {};

    std::ostringstream oss;
    for (auto byte : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

auto Base64UrlEncode(const std::vector<std::uint8_t>& data) -> std::string {
    int encoded_size = 4 * ((static_cast<int>(data.size()) + 2) / 3);
    std::vector<unsigned char> buf(encoded_size + 1);

    int actual = EVP_EncodeBlock(buf.data(), data.data(), static_cast<int>(data.size()));

    std::string result(reinterpret_cast<char*>(buf.data()), actual);
    for (auto& c : result) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }

    auto pos = result.find_last_not_of('=');
    if (pos != std::string::npos) {
        result.resize(pos + 1);
    }

    return result;
}

auto Base64UrlDecode(const std::string& input) -> std::vector<std::uint8_t> {
    std::string normalized = input;
    for (auto& c : normalized) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }

    auto eq_pos = normalized.find('=');
    if (eq_pos != std::string::npos) {
        for (size_t i = eq_pos; i < normalized.size(); ++i) {
            if (normalized[i] != '=') return {};
        }
        normalized.resize(eq_pos);
    }

    int padding = (4 - (static_cast<int>(normalized.size()) % 4)) % 4;
    normalized.append(padding, '=');

    std::vector<unsigned char> buf(normalized.size());
    int actual = EVP_DecodeBlock(buf.data(), reinterpret_cast<const unsigned char*>(normalized.data()),
                                 static_cast<int>(normalized.size()));

    if (actual < 0) return {};

    int result_len = (static_cast<int>(normalized.size()) / 4) * 3;
    if (padding >= 1) result_len -= 1;
    if (padding >= 2) result_len -= 2;

    buf.resize(std::max(0, result_len));
    return std::vector<std::uint8_t>(buf.begin(), buf.end());
}

JwtHandler::JwtHandler(std::string secret_key) : secret_key_(std::move(secret_key)) {
    if (secret_key_.empty()) secret_key_ = {};
}

auto JwtHandler::GenerateToken(const std::string& username, std::chrono::seconds ttl) const -> std::string {
    if (secret_key_.empty()) return {};
    auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_type("JWT")
        .set_subject(username)
        .set_issued_at(now)
        .set_expires_at(now + ttl)
        .sign(jwt::algorithm::hs256{secret_key_});
}

auto JwtHandler::ValidateToken(const std::string& token) const -> std::optional<std::string> {
    if (secret_key_.empty()) return std::nullopt;
    try {
        auto decoded = jwt::decode(token);
        if (!decoded.has_expires_at()) return std::nullopt;
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret_key_});
        verifier.verify(decoded);
        return decoded.get_subject();
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace qdb::server

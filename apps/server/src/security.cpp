#include <qdb/server/security.h>

#include <jwt-cpp/traits/nlohmann-json/defaults.h>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

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
    if (accounts_.find(username) != accounts_.end()) {
        return false;
    }

    Account account;
    account.username = username;
    account.salt = GenerateSalt();
    account.password_hash = HashPassword(password, account.salt);

    accounts_[username] = std::move(account);
    Save();
    return true;
}

auto AccountStore::Authenticate(const std::string& username, const std::string& password) -> bool {
    auto it = accounts_.find(username);
    if (it == accounts_.end()) {
        return false;
    }

    return it->second.password_hash == HashPassword(password, it->second.salt);
}

auto AccountStore::HasAccount(const std::string& username) -> bool {
    return accounts_.find(username) != accounts_.end();
}

void AccountStore::Load() {
    std::ifstream file(storage_path_);
    if (!file.is_open()) {
        return;
    }

    try {
        nlohmann::json j;
        file >> j;
        accounts_ = j.get<std::unordered_map<std::string, Account>>();
    } catch (...) {
        accounts_.clear();
    }
}

void AccountStore::Save() {
    nlohmann::json j = accounts_;
    std::ofstream file(storage_path_);
    if (file.is_open()) {
        file << j.dump(4);
    }
}

auto ComputeSha256(const std::vector<std::uint8_t>& data) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> result(EVP_MAX_MD_SIZE);
    unsigned int len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data.data(), data.size());
    EVP_DigestFinal_ex(ctx, result.data(), &len);
    EVP_MD_CTX_free(ctx);

    result.resize(len);
    return result;
}

auto ComputeHmacSha256(const std::vector<std::uint8_t>& key, const std::vector<std::uint8_t>& data) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> result(EVP_MAX_MD_SIZE);
    unsigned int len = 0;

    HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
         data.data(), data.size(), result.data(), &len);

    result.resize(len);
    return result;
}

auto GenerateSalt(size_t length) -> std::string {
    std::vector<unsigned char> bytes(length);
    RAND_bytes(bytes.data(), static_cast<int>(length));

    std::ostringstream oss;
    for (auto byte : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

auto HashPassword(const std::string& password, const std::string& salt) -> std::string {
    auto input = std::vector<std::uint8_t>(password.begin(), password.end());
    auto salt_bytes = std::vector<std::uint8_t>(salt.begin(), salt.end());
    input.insert(input.end(), salt_bytes.begin(), salt_bytes.end());

    auto hash = ComputeSha256(input);
    for (int i = 0; i < 10000; ++i) {
        hash = ComputeSha256(hash);
    }

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

    int padding = (4 - (normalized.size() % 4)) % 4;
    normalized.append(padding, '=');

    std::vector<unsigned char> buf(normalized.size());
    int actual = EVP_DecodeBlock(buf.data(), reinterpret_cast<const unsigned char*>(normalized.data()),
                                 static_cast<int>(normalized.size()));

    if (actual < 0) {
        return {};
    }

    int unpadded = static_cast<int>(input.size()) * 3 / 4;
    if (normalized.size() >= 2 && normalized[normalized.size() - 2] == '=') {
        unpadded -= 2;
    } else if (!normalized.empty() && normalized[normalized.size() - 1] == '=') {
        unpadded -= 1;
    }

    buf.resize(std::max(0, unpadded));
    return std::vector<std::uint8_t>(buf.begin(), buf.end());
}

JwtHandler::JwtHandler(std::string secret_key) : secret_key_(std::move(secret_key)) {}

auto JwtHandler::GenerateToken(const std::string& username, std::chrono::seconds ttl) -> std::string {
    auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_type("JWS")
        .set_subject(username)
        .set_issued_at(now)
        .set_expires_at(now + ttl)
        .sign(jwt::algorithm::hs256{secret_key_});
}

auto JwtHandler::ValidateToken(const std::string& token) -> std::optional<std::string> {
    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{secret_key_});
        verifier.verify(decoded);
        return decoded.get_subject();
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

}  // namespace qdb::server

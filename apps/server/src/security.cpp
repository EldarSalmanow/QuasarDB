#include <qdb/server/security.h>

#include <cctype>
#include <fstream>
#include <iomanip>
#include <random>
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

static const std::uint32_t kSha256K[] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static auto RotR(std::uint32_t x, int n) -> std::uint32_t { return (x >> n) | (x << (32 - n)); }
static auto Ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) -> std::uint32_t { return (x & y) ^ (~x & z); }
static auto Maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) -> std::uint32_t { return (x & y) ^ (x & z) ^ (y & z); }
static auto Sig0(std::uint32_t x) -> std::uint32_t { return RotR(x, 2) ^ RotR(x, 13) ^ RotR(x, 22); }
static auto Sig1(std::uint32_t x) -> std::uint32_t { return RotR(x, 6) ^ RotR(x, 11) ^ RotR(x, 25); }
static auto Sig2(std::uint32_t x) -> std::uint32_t { return RotR(x, 7) ^ RotR(x, 18) ^ (x >> 3); }
static auto Sig3(std::uint32_t x) -> std::uint32_t { return RotR(x, 17) ^ RotR(x, 19) ^ (x >> 10); }

Sha256::Sha256() : bit_count_(0), buffer_index_(0) {
    state_[0] = 0x6a09e667;
    state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372;
    state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f;
    state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab;
    state_[7] = 0x5be0cd19;
}

auto Sha256::Update(const std::vector<std::uint8_t>& data) -> void {
    for (auto byte : data) {
        buffer_[buffer_index_++] = byte;
        if (buffer_index_ == 64) {
            ProcessBlock(buffer_);
            buffer_index_ = 0;
        }
    }
    bit_count_ += data.size() * 8;
}

auto Sha256::Digest() -> std::vector<std::uint8_t> {
    buffer_[buffer_index_++] = 0x80;

    if (buffer_index_ > 56) {
        while (buffer_index_ < 64) {
            buffer_[buffer_index_++] = 0;
        }
        ProcessBlock(buffer_);
        buffer_index_ = 0;
    }

    while (buffer_index_ < 56) {
        buffer_[buffer_index_++] = 0;
    }

    for (int i = 7; i >= 0; --i) {
        buffer_[56 + i] = static_cast<std::uint8_t>(bit_count_ >> ((7 - i) * 8));
    }
    ProcessBlock(buffer_);

    std::vector<std::uint8_t> result(32);
    for (int i = 0; i < 8; ++i) {
        result[i * 4 + 0] = static_cast<std::uint8_t>(state_[i] >> 24);
        result[i * 4 + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
        result[i * 4 + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
        result[i * 4 + 3] = static_cast<std::uint8_t>(state_[i]);
    }

    return result;
}

auto Sha256::Hash(const std::vector<std::uint8_t>& data) -> std::vector<std::uint8_t> {
    Sha256 sha;
    sha.Update(data);
    return sha.Digest();
}

void Sha256::ProcessBlock(const std::uint8_t block[64]) {
    std::uint32_t w[64];

    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24)
             | (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16)
             | (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8)
             | (static_cast<std::uint32_t>(block[i * 4 + 3]));
    }

    for (int i = 16; i < 64; ++i) {
        w[i] = Sig3(w[i - 2]) + w[i - 7] + Sig2(w[i - 15]) + w[i - 16];
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];

    for (int i = 0; i < 64; ++i) {
        std::uint32_t t1 = h + Sig1(e) + Ch(e, f, g) + kSha256K[i] + w[i];
        std::uint32_t t2 = Sig0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

auto ComputeSha256(const std::vector<std::uint8_t>& data) -> std::vector<std::uint8_t> {
    return Sha256::Hash(data);
}

auto ComputeHmacSha256(const std::vector<std::uint8_t>& key, const std::vector<std::uint8_t>& data) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> k = key;
    if (k.size() > 64) {
        k = Sha256::Hash(k);
    }

    k.resize(64, 0);

    std::vector<std::uint8_t> opad(64, 0x5c);
    std::vector<std::uint8_t> ipad(64, 0x36);

    for (size_t i = 0; i < 64; ++i) {
        opad[i] ^= k[i];
        ipad[i] ^= k[i];
    }

    std::vector<std::uint8_t> inner_input = ipad;
    inner_input.insert(inner_input.end(), data.begin(), data.end());
    auto inner_hash = Sha256::Hash(inner_input);

    std::vector<std::uint8_t> outer_input = opad;
    outer_input.insert(outer_input.end(), inner_hash.begin(), inner_hash.end());
    return Sha256::Hash(outer_input);
}

auto GenerateSalt(size_t length) -> std::string {
    static constexpr char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    constexpr size_t char_count = sizeof(chars) - 1;

    std::random_device rd;
    std::string salt;
    salt.reserve(length);

    const auto max_acceptable = (std::random_device::max() / char_count) * char_count;
    for (size_t i = 0; i < length; ++i) {
        std::random_device::result_type value;
        do {
            value = rd();
        } while (value >= max_acceptable);

        salt += chars[value % char_count];
    }

    return salt;
}

auto HashPassword(const std::string& password, const std::string& salt) -> std::string {
    std::vector<std::uint8_t> input(password.begin(), password.end());
    std::vector<std::uint8_t> salt_bytes(salt.begin(), salt.end());
    input.insert(input.end(), salt_bytes.begin(), salt_bytes.end());

    auto hash = Sha256::Hash(input);
    for (int i = 0; i < 10000; ++i) {
        hash = Sha256::Hash(hash);
    }

    std::ostringstream oss;
    for (auto byte : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

static const std::string kBase64UrlChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

auto Base64UrlEncode(const std::vector<std::uint8_t>& data) -> std::string {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    for (size_t i = 0; i < data.size(); i += 3) {
        std::uint32_t triple = 0;
        int remaining = 0;

        for (int j = 0; j < 3; ++j) {
            if (i + j < data.size()) {
                triple = (triple << 8) | data[i + j];
                ++remaining;
            } else {
                triple <<= 8;
            }
        }

        result += kBase64UrlChars[(triple >> 18) & 0x3f];
        result += kBase64UrlChars[(triple >> 12) & 0x3f];
        result += (remaining > 1) ? kBase64UrlChars[(triple >> 6) & 0x3f] : '=';
        result += (remaining > 2) ? kBase64UrlChars[triple & 0x3f] : '=';
    }

    return result;
}

auto Base64UrlDecode(const std::string& input) -> std::vector<std::uint8_t> {
    std::vector<std::uint8_t> result;

    std::string normalized;
    for (auto c : input) {
        if (c == '=') break;
        if (c == '-' || c == '_' || std::isalnum(c)) {
            normalized += c;
        }
    }

    result.reserve((normalized.size() * 3) / 4);

    for (size_t i = 0; i < normalized.size(); i += 4) {
        std::uint32_t tetra = 0;

        for (int j = 0; j < 4; ++j) {
            tetra <<= 6;
            if (i + j < normalized.size()) {
                char c = normalized[i + j];
                if (c >= 'A' && c <= 'Z') tetra |= (c - 'A');
                else if (c >= 'a' && c <= 'z') tetra |= (c - 'a' + 26);
                else if (c >= '0' && c <= '9') tetra |= (c - '0' + 52);
                else if (c == '-') tetra |= 62;
                else if (c == '_') tetra |= 63;
            }
        }

        result.push_back(static_cast<std::uint8_t>((tetra >> 16) & 0xff));
        if (i + 2 < normalized.size()) {
            result.push_back(static_cast<std::uint8_t>((tetra >> 8) & 0xff));
        }
        if (i + 3 < normalized.size()) {
            result.push_back(static_cast<std::uint8_t>(tetra & 0xff));
        }
    }

    return result;
}

JwtHandler::JwtHandler(std::string secret_key) : secret_key_(std::move(secret_key)) {}

auto JwtHandler::GenerateToken(const std::string& username, std::chrono::seconds ttl) -> std::string {
    auto header = nlohmann::json{{"alg", "HS256"}, {"typ", "JWT"}};
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto exp_val = now + ttl.count();

    auto payload = nlohmann::json{
        {"sub", username},
        {"iat", now},
        {"exp", exp_val}
    };

    auto hdr_str = header.dump();
    auto pld_str = payload.dump();

    std::vector<std::uint8_t> hdr_vec(hdr_str.begin(), hdr_str.end());
    auto header_b64 = Base64UrlEncode(hdr_vec);

    std::vector<std::uint8_t> pld_vec(pld_str.begin(), pld_str.end());
    auto payload_b64 = Base64UrlEncode(pld_vec);

    auto signing_input = header_b64 + "." + payload_b64;
    std::vector<std::uint8_t> secret_vec(secret_key_.begin(), secret_key_.end());
    std::vector<std::uint8_t> input_vec(signing_input.begin(), signing_input.end());
    auto signature = ComputeHmacSha256(secret_vec, input_vec);
    auto signature_b64 = Base64UrlEncode(signature);

    return header_b64 + "." + payload_b64 + "." + signature_b64;
}

auto JwtHandler::ValidateToken(const std::string& token) -> std::optional<std::string> {
    auto dot1 = token.find('.');
    if (dot1 == std::string::npos) return std::nullopt;
    auto dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) return std::nullopt;

    auto header_b64 = token.substr(0, dot1);
    auto payload_b64 = token.substr(dot1 + 1, dot2 - dot1 - 1);
    auto signature_b64 = token.substr(dot2 + 1);

    auto signing_input = header_b64 + "." + payload_b64;
    auto secret_bytes = std::vector<std::uint8_t>(secret_key_.begin(), secret_key_.end());
    auto input_bytes = std::vector<std::uint8_t>(signing_input.begin(), signing_input.end());
    auto expected_sig = ComputeHmacSha256(secret_bytes, input_bytes);
    auto actual_sig = Base64UrlDecode(signature_b64);

    if (expected_sig != actual_sig) {
        return std::nullopt;
    }

    auto payload_bytes = Base64UrlDecode(payload_b64);
    auto payload_str = std::string(payload_bytes.begin(), payload_bytes.end());

    try {
        auto payload = nlohmann::json::parse(payload_str);
        auto exp = payload.value("exp", 0LL);
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        if (now > exp) {
            return std::nullopt;
        }

        if (!payload.contains("sub") || !payload["sub"].is_string()) {
            return std::nullopt;
        }

        return payload["sub"].get<std::string>();
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace qdb::server

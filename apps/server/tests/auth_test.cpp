#include <qdb/server/security.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

namespace qdb::server {

TEST(AuthTest, Sha256Basic) {
    auto empty_hash = ComputeSha256({});
    ASSERT_EQ(empty_hash.size(), 32);

    std::string input = "hello";
    auto data = std::vector<std::uint8_t>(input.begin(), input.end());
    auto hash = ComputeSha256(data);
    ASSERT_EQ(hash.size(), 32);

    std::string expected_hex =
        "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    std::ostringstream oss;
    for (auto byte : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    ASSERT_EQ(oss.str(), expected_hex);
}

TEST(AuthTest, Sha256Empty) {
    auto hash = ComputeSha256({});
    ASSERT_EQ(hash.size(), 32);

    std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    std::ostringstream oss;
    for (auto byte : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    ASSERT_EQ(oss.str(), expected);
}

TEST(AuthTest, HmacSha256Basic) {
    std::string key_str = "key";
    std::string data_str = "The quick brown fox jumps over the lazy dog";
    auto key = std::vector<std::uint8_t>(key_str.begin(), key_str.end());
    auto data = std::vector<std::uint8_t>(data_str.begin(), data_str.end());
    auto hmac = ComputeHmacSha256(key, data);
    ASSERT_EQ(hmac.size(), 32);

    std::string expected = "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8";
    std::ostringstream oss;
    for (auto byte : hmac) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    ASSERT_EQ(oss.str(), expected);
}

TEST(AuthTest, Base64UrlEncode) {
    std::string input = "test";
    auto data = std::vector<std::uint8_t>(input.begin(), input.end());
    auto encoded = Base64UrlEncode(data);
    ASSERT_EQ(encoded, "dGVzdA");
}

TEST(AuthTest, Base64UrlRoundtrip) {
    std::string input = "Hello, World! This is a test with URL unsafe chars: +/";
    auto data = std::vector<std::uint8_t>(input.begin(), input.end());
    auto encoded = Base64UrlEncode(data);
    ASSERT_TRUE(std::none_of(encoded.begin(), encoded.end(), [](char c) {
        return c == '+' || c == '/' || c == '=';
    }));

    auto decoded = Base64UrlDecode(encoded);
    auto result = std::string(decoded.begin(), decoded.end());
    ASSERT_EQ(result, input);
}

TEST(AuthTest, PasswordHashing) {
    auto salt = GenerateSalt();
    ASSERT_EQ(salt.size(), 32);

    auto hash1 = HashPassword("password123", salt);
    auto hash2 = HashPassword("password123", salt);
    auto hash3 = HashPassword("different", salt);

    ASSERT_EQ(hash1, hash2);
    ASSERT_NE(hash1, hash3);
}

TEST(AuthTest, GenerateSaltUnique) {
    auto salt1 = GenerateSalt();
    auto salt2 = GenerateSalt();
    ASSERT_NE(salt1, salt2);
}

TEST(AuthTest, JwtGenerateAndValidate) {
    JwtHandler jwt("test-secret-key");
    auto token = jwt.GenerateToken("testuser");
    ASSERT_FALSE(token.empty());

    auto result = jwt.ValidateToken(token);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value(), "testuser");
}

TEST(AuthTest, JwtInvalidSignature) {
    JwtHandler jwt1("secret1");
    JwtHandler jwt2("secret2");

    auto token = jwt1.GenerateToken("testuser");
    auto result = jwt2.ValidateToken(token);
    ASSERT_FALSE(result.has_value());
}

TEST(AuthTest, JwtTamperedToken) {
    JwtHandler jwt("secret");

    auto token = jwt.GenerateToken("testuser");
    auto dot_pos = token.find('.');
    ASSERT_NE(dot_pos, std::string::npos);

    auto tampered = "AAAA" + token.substr(dot_pos);
    auto result = jwt.ValidateToken(tampered);
    ASSERT_FALSE(result.has_value());
}

TEST(AuthTest, AccountStoreCreate) {
    auto test_path = "test_accounts.json";
    std::remove(test_path);

    {
        AccountStore store(test_path);
        ASSERT_TRUE(store.CreateAccount("alice", "pass123"));
        ASSERT_FALSE(store.CreateAccount("alice", "other"));
    }

    std::remove(test_path);
}

TEST(AuthTest, AccountStoreAuthenticate) {
    auto test_path = "test_accounts_auth.json";
    std::remove(test_path);

    {
        AccountStore store(test_path);
        store.CreateAccount("bob", "secure_pass");

        ASSERT_TRUE(store.Authenticate("bob", "secure_pass"));
        ASSERT_FALSE(store.Authenticate("bob", "wrong_pass"));
        ASSERT_FALSE(store.Authenticate("nonexistent", "pass"));
    }

    std::remove(test_path);
}

TEST(AuthTest, AccountStorePersistence) {
    auto test_path = "test_accounts_persist.json";
    std::remove(test_path);

    {
        AccountStore store(test_path);
        store.CreateAccount("persist_user", "my_password");
        ASSERT_TRUE(store.HasAccount("persist_user"));
    }

    {
        AccountStore store(test_path);
        ASSERT_TRUE(store.HasAccount("persist_user"));
        ASSERT_TRUE(store.Authenticate("persist_user", "my_password"));
    }

    std::remove(test_path);
}

TEST(AuthTest, AccountStoreHasAccount) {
    auto test_path = "test_accounts_has.json";
    std::remove(test_path);

    {
        AccountStore store(test_path);
        ASSERT_FALSE(store.HasAccount("nobody"));
        store.CreateAccount("someone", "pass");
        ASSERT_TRUE(store.HasAccount("someone"));
    }

    std::remove(test_path);
}

TEST(AuthTest, JwtExpiredToken) {
    JwtHandler jwt("secret");

    auto token = jwt.GenerateToken("user", std::chrono::seconds(-1));
    auto result = jwt.ValidateToken(token);
    ASSERT_FALSE(result.has_value());
}

TEST(AuthTest, Sha256Deterministic) {
    std::string input = "deterministic_test_data_123!@#";
    auto data = std::vector<std::uint8_t>(input.begin(), input.end());

    auto hash1 = ComputeSha256(data);
    auto hash2 = ComputeSha256(data);

    ASSERT_EQ(hash1, hash2);
}

TEST(AuthTest, SaltChangesHash) {
    auto hash1 = HashPassword("same_password", "salt1");
    auto hash2 = HashPassword("same_password", "salt2");
    ASSERT_NE(hash1, hash2);
}

TEST(AuthTest, AccountStoreMultipleUsers) {
    auto test_path = "test_accounts_multi.json";
    std::remove(test_path);

    {
        AccountStore store(test_path);
        ASSERT_TRUE(store.CreateAccount("user1", "pass1"));
        ASSERT_TRUE(store.CreateAccount("user2", "pass2"));
        ASSERT_TRUE(store.CreateAccount("user3", "pass3"));

        ASSERT_TRUE(store.Authenticate("user1", "pass1"));
        ASSERT_TRUE(store.Authenticate("user2", "pass2"));
        ASSERT_TRUE(store.Authenticate("user3", "pass3"));

        ASSERT_FALSE(store.Authenticate("user1", "wrong"));
    }

    std::remove(test_path);
}

}  // namespace qdb::server

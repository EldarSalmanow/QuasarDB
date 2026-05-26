#include <qdb/client/config.h>

#include <gtest/gtest.h>

#include <initializer_list>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "test_support.h"

namespace qdb::client::test {
namespace {

auto Parse(std::initializer_list<const char*> args) -> std::optional<Config> {
    std::vector<char*> argv;
    argv.reserve(args.size());

    for (const char* arg : args) {
        argv.push_back(const_cast<char*>(arg));
    }

    return Config::FromArguments(static_cast<int>(argv.size()), argv.data());
}

}  // namespace

TEST(ConfigTest, ParsesDefaults) {
    auto config = Parse({"qdb-client"});
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->Host(), "127.0.0.1");
    EXPECT_EQ(config->Port(), 9000u);
    EXPECT_EQ(config->File(), "");
}

TEST(ConfigTest, ParsesCustomArguments) {
    auto config = Parse({"qdb-client", "--host", "127.0.0.1", "-p", "6000", "-f", "script.sql"});
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->Host(), "127.0.0.1");
    EXPECT_EQ(config->Port(), 6000u);
    EXPECT_EQ(config->File(), "script.sql");
}

TEST(ConfigTest, HelpPrintsUsageAndReturnsNullopt) {
    OutputCapture capture(std::cout);

    auto config = Parse({"qdb-client", "--help"});

    EXPECT_FALSE(config.has_value());
    EXPECT_TRUE(Contains(capture.Str(), "QuasarDB client"));
    EXPECT_TRUE(Contains(capture.Str(), "--host"));
    EXPECT_TRUE(Contains(capture.Str(), "--port"));
    EXPECT_TRUE(Contains(capture.Str(), "--file"));
}

TEST(ConfigTest, InvalidPortThrows) {
    EXPECT_THROW((Parse({"qdb-client", "-p", "0"})), std::runtime_error);
}

}  // namespace qdb::client::test


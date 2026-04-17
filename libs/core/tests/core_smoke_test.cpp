// #include <args.hxx>
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

TEST(CoreConanDeps, HeadersAreAvailable) {
    nlohmann::json payload = {{"status", "ok"}};
    EXPECT_EQ(payload.at("status"), "ok");

    // args::ArgumentParser parser("qdb test parser");
    // EXPECT_NO_THROW(static_cast<void>(parser));
}

// TEST(CoreContracts, VersionHeaderCompiles) {
//     const qdb::core::Version version{0, 1, 0};
//     EXPECT_EQ(version.major, 0);
//     EXPECT_EQ(version.minor, 1);
//     EXPECT_EQ(version.patch, 0);
// }

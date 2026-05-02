#include <gtest/gtest.h>
#include <qdb/client/application.h>
#include <qdb/client/config.h>
#include <qdb/client/reader.h>
#include <qdb/client/renderer.h>
#include <qdb/client/request.h>
#include <qdb/client/stream.h>

#include <type_traits>

TEST(ClientContracts, HeadersCompile) {
    static_assert(std::is_class_v<qdb::client::Application>);
    static_assert(std::is_class_v<qdb::client::Config>);
    static_assert(std::is_class_v<qdb::client::IInputStream>);
    static_assert(std::is_class_v<qdb::client::ConsoleReader>);
    static_assert(std::is_class_v<qdb::client::FileReader>);
    static_assert(std::is_class_v<qdb::client::Request>);
    static_assert(std::is_class_v<qdb::client::Response>);

    SUCCEED();
}

TEST(ClientBasic, AlwaysPasses) { EXPECT_TRUE(true); }

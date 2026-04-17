#include <gtest/gtest.h>
#include <qdb/client/application.h>
#include <qdb/client/config.h>
#include <qdb/client/connection.h>
#include <qdb/client/reader.h>
#include <qdb/client/renderer.h>
#include <qdb/client/request.h>
#include <qdb/client/stream.h>
#include <qdb/core/version.h>

#include <type_traits>

TEST(ClientContracts, HeadersCompile) {
    static_assert(std::is_class_v<qdb::client::Application>);
    static_assert(std::is_class_v<qdb::client::Config>);
    static_assert(std::is_class_v<qdb::client::Connection>);
    static_assert(std::is_class_v<qdb::client::IReader>);
    static_assert(std::is_class_v<qdb::client::ConsoleReader>);
    static_assert(std::is_class_v<qdb::client::Request>);
    static_assert(std::is_class_v<qdb::client::Response>);
    // static_assert(std::is_class_v<qdb::client::Session>);
    // static_assert(std::is_class_v<qdb::client::Stream>);
    // static_assert(std::is_class_v<qdb::core::Version>);

    SUCCEED();
}

#include <qdb/client/renderer.h>

#include <gtest/gtest.h>

#include <string>

#include "test_support.h"

namespace qdb::client::test {

TEST(RendererTest, WelcomeContainsExpectedText) {
    Renderer renderer;
    OutputCapture capture(std::cout);

    renderer.RenderWelcome();

    const auto output = capture.Str();
    EXPECT_TRUE(Contains(output, "QuasarDB Client v1.0"));
    EXPECT_TRUE(Contains(output, "Type your SQL queries and press Enter. End with ';'"));
}

TEST(RendererTest, ErrorResponseIsPrintedInRed) {
    Renderer renderer;
    OutputCapture capture(std::cout);

    renderer.RenderError("Failed to connect");

    EXPECT_EQ(capture.Str(), "\033[31mError: Failed to connect\033[0m\n");
}

TEST(RendererTest, ErrorResponseRenderedViaResponseObject) {
    Renderer renderer;
    const auto response = qdb::core::ResponseBuilder::Error()
                              .Code("SYNTAX_ERROR")
                              .Message("Bad query")
                              .Build();
    OutputCapture capture(std::cout);

    renderer.RenderResponse(response);

    EXPECT_EQ(capture.Str(), "\033[31mError [SYNTAX_ERROR]: Bad query\033[0m\n");
}

TEST(RendererTest, SuccessfulSelectResponseRendersTable) {
    Renderer renderer;
    const auto response = qdb::core::ResponseBuilder::Ok()
                             .Code("SUCCESS")
                             .Message("Query executed")
                             .Data(R"([{"id":1,"name":"Alice"},{"id":2,"name":"Bob"}])")
                             .Build();
    OutputCapture capture(std::cout);

    renderer.RenderResponse(response);

    const auto output = capture.Str();
    EXPECT_TRUE(Contains(output, "Query executed"));
    EXPECT_TRUE(Contains(output, "id"));
    EXPECT_TRUE(Contains(output, "name"));
    EXPECT_TRUE(Contains(output, "Alice"));
    EXPECT_TRUE(Contains(output, "Bob"));
    EXPECT_TRUE(Contains(output, "row(s) returned"));
}

TEST(RendererTest, JsonObjectIsPrintedPretty) {
    Renderer renderer;
    const auto response = qdb::core::ResponseBuilder::Ok()
                             .Code("SUCCESS")
                             .Message("Insert successful")
                             .Data(R"({"affected_rows":1})")
                             .Build();
    OutputCapture capture(std::cout);

    renderer.RenderResponse(response);

    const auto output = capture.Str();
    EXPECT_TRUE(Contains(output, "Insert successful"));
    EXPECT_TRUE(Contains(output, "affected_rows"));
    EXPECT_TRUE(Contains(output, "1"));
}

}  // namespace qdb::client::test



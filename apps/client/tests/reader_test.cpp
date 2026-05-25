#include <qdb/client/reader.h>

#include <gtest/gtest.h>

#include <string>

#include "test_support.h"

namespace qdb::client::test {

TEST(ConsoleReaderTest, ReadsSingleLineAndPrintsPrompt) {
    InputRedirect input("  SELECT 1;\n");
    OutputCapture capture(std::cout);

    ConsoleReader reader;
    auto command = reader.ReadCommand();

    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT 1;");
    EXPECT_EQ(capture.Str(), "qdb> ");
}

TEST(ConsoleReaderTest, EmptyLineReturnsEmptyCommand) {
    InputRedirect input("\n");

    ConsoleReader reader;
    auto command = reader.ReadCommand();

    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "");
}

TEST(ConsoleReaderTest, ReadsMultilineCommandUntilSemicolon) {
    InputRedirect input("SELECT *\nFROM users\nWHERE id == 1;\n");
    OutputCapture capture(std::cout);

    ConsoleReader reader;
    auto command = reader.ReadCommand();

    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT *\nFROM users\nWHERE id == 1;");
    EXPECT_EQ(capture.Str(), "qdb> ...> ...> ");
}

TEST(FileReaderTest, ReadsTrimmedLinesOneByOne) {
    ScopedTempFile file("  SELECT * FROM users;  \n\n   INSERT INTO users VALUES (1);\n");

    FileReader reader(file.Path().string());

    auto first = reader.ReadCommand();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first.value(), "SELECT * FROM users;");
    EXPECT_TRUE(reader.HasMore());

    auto second = reader.ReadCommand();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second.value(), "INSERT INTO users VALUES (1);");
    EXPECT_FALSE(reader.HasMore());

    auto third = reader.ReadCommand();
    EXPECT_FALSE(third.has_value());
}

TEST(FileReaderTest, ReadsMultilineStatements) {
    ScopedTempFile file("CREATE TABLE users (\n  id INT,\n  name STRING\n);\nSELECT * FROM users;\n");
    FileReader reader(file.Path().string());

    auto first = reader.ReadCommand();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first.value(), "CREATE TABLE users (\n  id INT,\n  name STRING\n);");

    auto second = reader.ReadCommand();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second.value(), "SELECT * FROM users;");
}

TEST(FileReaderTest, MissingFileHasNoCommands) {
    FileReader reader("definitely_missing_file.sql");

    EXPECT_FALSE(reader.HasMore());
    EXPECT_FALSE(reader.ReadCommand().has_value());
}

}  // namespace qdb::client::test

#include <qdb/client/reader.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace qdb::client::test {

// Test FileReader with single line command
TEST(FileReaderTest, SingleLineCommand) {
    // Create temporary test file
    const std::string test_file = "test_single_line.sql";
    {
        std::ofstream out(test_file);
        out << "SELECT * FROM users;";
    }

    FileReader reader(test_file);
    
    auto command = reader.ReadCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT id, name, email FROM users WHERE active = 1");
    
    command = reader.ReadCommand();
    EXPECT_FALSE(command.has_value());
    EXPECT_FALSE(reader.hasMore());

    // Cleanup
    std::remove(test_file.c_str());
}

// Test FileReader with multiline command
TEST(FileReaderTest, MultilineCommand) {
    const std::string test_file = "test_multiline.sql";
    {
        std::ofstream out(test_file);
        out << "SELECT id, name, email\n";
        out << "FROM users\n";
        out << "WHERE active = 1;";
    }

    FileReader reader(test_file);
    
    auto command = reader.ReadCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT * FROM users");
    
    command = reader.ReadCommand();
    EXPECT_FALSE(command.has_value());
    EXPECT_FALSE(reader.HasMore());
}

// Test FileReader with command without semicolon at EOF
TEST(FileReaderTest, CommandWithoutSemicolonAtEOF) {
    const std::string test_file = "test_no_semicolon.sql";
    {
        std::ofstream out(test_file);
        out << "SELECT * FROM users";
    }

    FileReader reader(test_file);
    
    auto command = reader.ReadCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT * FROM users");

    std::remove(test_file.c_str());
}

// Test FileReader with complex multiline query
TEST(FileReaderTest, ComplexMultilineQuery) {
    const std::string test_file = "test_complex.sql";
    {
        std::ofstream out(test_file);
        out << "INSERT INTO users\n";
        out << "  (id, name, email)\n";
        out << "VALUES\n";
        out << "  (1, 'John Doe', 'john@example.com');";
    }

    FileReader reader(test_file);
    
    auto command = reader.ReadCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "INSERT INTO users (id, name, email) VALUES (1, 'John Doe', 'john@example.com')");

    std::remove(test_file.c_str());
}

}  // namespace qdb::client::test

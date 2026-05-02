#include <qdb/client/reader.h>

#include <gtest/gtest.h>

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
    
    auto command = reader.readCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT * FROM users");
    
    // No more commands
    command = reader.readCommand();
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
    
    auto command = reader.readCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT id, name, email FROM users WHERE active = 1");
    
    command = reader.readCommand();
    EXPECT_FALSE(command.has_value());

    std::remove(test_file.c_str());
}

// Test FileReader with multiple commands
TEST(FileReaderTest, MultipleCommands) {
    const std::string test_file = "test_multiple.sql";
    {
        std::ofstream out(test_file);
        out << "CREATE DATABASE testdb;\n";
        out << "USE testdb;\n";
        out << "CREATE TABLE users (id INT, name STRING);";
    }

    FileReader reader(test_file);
    
    auto cmd1 = reader.readCommand();
    ASSERT_TRUE(cmd1.has_value());
    EXPECT_EQ(cmd1.value(), "CREATE DATABASE testdb");
    
    auto cmd2 = reader.readCommand();
    ASSERT_TRUE(cmd2.has_value());
    EXPECT_EQ(cmd2.value(), "USE testdb");
    
    auto cmd3 = reader.readCommand();
    ASSERT_TRUE(cmd3.has_value());
    EXPECT_EQ(cmd3.value(), "CREATE TABLE users (id INT, name STRING)");
    
    auto cmd4 = reader.readCommand();
    EXPECT_FALSE(cmd4.has_value());

    std::remove(test_file.c_str());
}

// Test FileReader with empty lines
TEST(FileReaderTest, EmptyLines) {
    const std::string test_file = "test_empty_lines.sql";
    {
        std::ofstream out(test_file);
        out << "\n";
        out << "SELECT * FROM users;\n";
        out << "\n";
        out << "\n";
        out << "SELECT * FROM orders;";
    }

    FileReader reader(test_file);
    
    auto cmd1 = reader.readCommand();
    ASSERT_TRUE(cmd1.has_value());
    EXPECT_EQ(cmd1.value(), "SELECT * FROM users");
    
    auto cmd2 = reader.readCommand();
    ASSERT_TRUE(cmd2.has_value());
    EXPECT_EQ(cmd2.value(), "SELECT * FROM orders");

    std::remove(test_file.c_str());
}

// Test FileReader with whitespace trimming
TEST(FileReaderTest, WhitespaceTrimming) {
    const std::string test_file = "test_whitespace.sql";
    {
        std::ofstream out(test_file);
        out << "   SELECT * FROM users   ;   \n";
    }

    FileReader reader(test_file);
    
    auto command = reader.readCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT * FROM users");

    std::remove(test_file.c_str());
}

// Test FileReader with non-existent file
TEST(FileReaderTest, NonExistentFile) {
    FileReader reader("non_existent_file.sql");
    
    auto command = reader.readCommand();
    EXPECT_FALSE(command.has_value());
    EXPECT_FALSE(reader.hasMore());
}

// Test FileReader with command without semicolon at EOF
TEST(FileReaderTest, CommandWithoutSemicolonAtEOF) {
    const std::string test_file = "test_no_semicolon.sql";
    {
        std::ofstream out(test_file);
        out << "SELECT * FROM users";
    }

    FileReader reader(test_file);
    
    auto command = reader.readCommand();
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
    
    auto command = reader.readCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "INSERT INTO users (id, name, email) VALUES (1, 'John Doe', 'john@example.com')");

    std::remove(test_file.c_str());
}

}  // namespace qdb::client::test

#include <qdb/client/reader.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace qdb::client::test {

class StreamBufGuard {
public:
    StreamBufGuard(std::istream& stream, std::streambuf* new_buf) : stream_(stream), old_buf_(stream.rdbuf(new_buf)) {}
    StreamBufGuard(std::ostream& stream, std::streambuf* new_buf) : ostream_(&stream), old_buf_(stream.rdbuf(new_buf)) {}

    ~StreamBufGuard() {
        if (ostream_) {
            ostream_->rdbuf(old_buf_);
        } else {
            stream_.rdbuf(old_buf_);
        }
    }

private:
    std::istream& stream_{std::cin};
    std::ostream* ostream_{nullptr};
    std::streambuf* old_buf_;
};

TEST(FileReaderTest, SingleLineCommand) {
    const std::string test_file = "test_single_line.sql";
    {
        std::ofstream out(test_file);
        out << "SELECT * FROM users;";
    }

    FileReader reader(test_file);
    
    auto command = reader.ReadCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT * FROM users");
    
    command = reader.ReadCommand();
    EXPECT_FALSE(command.has_value());
    EXPECT_FALSE(reader.HasMore());

    std::remove(test_file.c_str());
}

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
    EXPECT_EQ(command.value(), "SELECT id, name, email FROM users WHERE active = 1");
    
    command = reader.ReadCommand();
    EXPECT_FALSE(command.has_value());
    EXPECT_FALSE(reader.HasMore());
}

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

TEST(FileReaderTest, MultipleCommands) {
    const std::string test_file = "test_multiple.sql";
    {
        std::ofstream out(test_file);
        out << "SELECT 1;\n";
        out << "\n";
        out << "SELECT 2;\n";
    }

    FileReader reader(test_file);

    auto command = reader.ReadCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT 1");
    EXPECT_TRUE(reader.HasMore());

    command = reader.ReadCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT 2");

    command = reader.ReadCommand();
    EXPECT_FALSE(command.has_value());

    std::remove(test_file.c_str());
}

TEST(FileReaderTest, SemicolonOnlyLinesIgnored) {
    const std::string test_file = "test_semicolon_only.sql";
    {
        std::ofstream out(test_file);
        out << ";\n";
        out << "   ;   \n";
        out << "SELECT 42;\n";
    }

    FileReader reader(test_file);

    auto command = reader.ReadCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT 42");

    std::remove(test_file.c_str());
}

TEST(ConsoleReaderTest, MultilineCommandAndEof) {
    std::istringstream input("SELECT\n1;\n");
    std::ostringstream output;

    StreamBufGuard cin_guard(std::cin, input.rdbuf());
    StreamBufGuard cout_guard(std::cout, output.rdbuf());

    ConsoleReader reader;
    auto command = reader.ReadCommand();
    ASSERT_TRUE(command.has_value());
    EXPECT_EQ(command.value(), "SELECT 1");

    command = reader.ReadCommand();
    EXPECT_FALSE(command.has_value());
    EXPECT_FALSE(reader.HasMore());
}

}

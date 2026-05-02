#ifndef QUASARDB_CLIENT_READER_H
#define QUASARDB_CLIENT_READER_H

#include <qdb/client/stream.h>

#include <fstream>
#include <string>

namespace qdb::client {

/**
 * @brief Console reader for interactive REPL mode
 * 
 * Reads from stdin with multiline support until semicolon.
 * Displays prompts: "quasar> " for first line, "      -> " for continuation.
 */
class ConsoleReader : public IInputStream {
public:
    ConsoleReader() = default;
    ~ConsoleReader() override = default;

    std::optional<std::string> ReadCommand() override;
    bool HasMore() const override;
};

/**
 * @brief File reader for batch mode
 * 
 * Reads SQL commands from a file with multiline support until semicolon.
 */
class FileReader : public IInputStream {
public:
    explicit FileReader(const std::string& file_path);
    ~FileReader() override = default;

    std::optional<std::string> ReadCommand() override;
    bool HasMore() const override;

private:
    std::string file_path_;
    std::ifstream file_stream_;
    bool is_open_ = false;
};

}  // namespace qdb::client

#endif  // QUASARDB_CLIENT_READER_H

#ifndef QUASARDB_CLIENT_READER_H
#define QUASARDB_CLIENT_READER_H

#include <qdb/client/stream.h>

#include <fstream>
#include <string>

namespace qdb::client {

class ConsoleReader : public IInputStream {
public:
    ConsoleReader() = default;
    ~ConsoleReader() override = default;

    std::optional<std::string> ReadCommand() override;
    bool HasMore() const override;
};

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

#endif

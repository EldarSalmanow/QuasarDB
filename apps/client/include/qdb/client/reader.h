#ifndef QUASARDB_READER_H
#define QUASARDB_READER_H

#include <fstream>
#include <optional>
#include <string>

namespace qdb::client {

class IReader {
public:
    virtual ~IReader();

public:
    virtual std::optional<std::string> ReadCommand() = 0;

    virtual bool HasMore() const = 0;
};

class ConsoleReader : public IReader {
public:
    ConsoleReader();

public:
    std::optional<std::string> ReadCommand() override;

    bool HasMore() const override;
};

class FileReader : public IReader {
public:
    explicit FileReader(const std::string &file_path);

public:
    ~FileReader() override;

public:
    std::optional<std::string> ReadCommand() override;

    bool HasMore() const override;

private:
    std::string file_path_;

    std::ifstream file_stream_;
};

}  // namespace qdb::client

#endif  // QUASARDB_READER_H

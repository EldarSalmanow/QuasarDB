#ifndef QUASARDB_STREAM_H
#define QUASARDB_STREAM_H

#include <string>

namespace qdb::client {

    class IReader {
    public:
        virtual ~IReader();

    public:
        virtual bool Open() = 0;
        virtual void Close() = 0;
        virtual std::string ReadLine() = 0;
        virtual bool HasNext() const = 0;
    };

    // for REPL
    class ConsoleReader : public IReader {
    public:
        bool Open() override;
        void Close() override;
        std::string ReadLine() override;
        bool HasNext() const override;
    };

    class FileReader : public IReader {
    public:
        explicit FileReader(const std::string& file_path);

    public:
        bool Open() override;
        void Close() override;
        std::string ReadLine() override;
        bool HasNext() const override;
    };

} // namespace qdb::client

#endif  // QUASARDB_STREAM_H

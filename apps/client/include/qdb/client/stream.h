#ifndef QUASARDB_CLIENT_STREAM_H
#define QUASARDB_CLIENT_STREAM_H

#include <optional>
#include <string>

namespace qdb::client {

/**
 * @brief Interface for input streams that read SQL commands
 * 
 * Commands are accumulated until a semicolon (;) is encountered.
 * Supports multiline input.
 */
class IInputStream {
public:
    virtual ~IInputStream() = default;

    /**
     * @brief Read a complete command (until semicolon)
     * @return Command string without trailing semicolon, or std::nullopt if no more input
     */
    virtual std::optional<std::string> readCommand() = 0;

    /**
     * @brief Check if there is more input available
     * @return true if more input can be read, false otherwise
     */
    virtual bool hasMore() const = 0;
};

}  // namespace qdb::client

#endif  // QUASARDB_CLIENT_STREAM_H

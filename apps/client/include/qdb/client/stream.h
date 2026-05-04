#ifndef QUASARDB_CLIENT_STREAM_H
#define QUASARDB_CLIENT_STREAM_H

#include <optional>
#include <string>

namespace qdb::client {

class IInputStream {
public:
    virtual ~IInputStream() = default;

    virtual std::optional<std::string> ReadCommand() = 0;

    virtual bool HasMore() const = 0;
};

}  // namespace qdb::client

#endif

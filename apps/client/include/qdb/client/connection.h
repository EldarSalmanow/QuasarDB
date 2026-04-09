#ifndef QUASARDB_CONNECTION_H
#define QUASARDB_CONNECTION_H

#include <qdb/client/request.h>
#include <qdb/client/response.h>

namespace qdb::client {

    class Connection {
    public:
        auto Connect(const std::string &host, int port) -> bool;

        auto Disconnect() -> void;

        auto IsConnected() const -> bool;

        auto Send(const Request &request) -> Response;
    };

} // namespace qdb::client

#endif  // QUASARDB_CONNECTION_H

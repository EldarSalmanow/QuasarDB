#ifndef QUASARDB_CLIENT_CONNECTION_H
#define QUASARDB_CLIENT_CONNECTION_H

#include <qdb/client/request.h>
#include <qdb/client/response.h>
#include <qdb/core/tcp_client.h>

#include <memory>
#include <string>

namespace qdb::client {

class Connection {
public:
    Connection(const std::string& host, uint32_t port);
    ~Connection() = default;

    bool Connect();
    void Disconnect();
    bool IsConnected() const;

    Response ExecuteQuery(const std::string& sql, const std::string& token = "");
    Response SendRequest(const Request& request);

private:
    std::unique_ptr<qdb::core::TcpClient> client_;
    std::string host_;
    uint32_t port_;
};

}  // namespace qdb::client

#endif  // QUASARDB_CLIENT_CONNECTION_H
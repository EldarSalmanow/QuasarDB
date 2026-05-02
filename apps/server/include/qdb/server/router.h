//
// Created by eldar on 09.04.2026.
//

#ifndef QUASARDB_ROUTER_H
#define QUASARDB_ROUTER_H

namespace qdb::server {

class Router {
public:
    Router() = default;

    ~Router() = default;

public:
    auto Process() -> std::optional<nlohmann::json>;

private:
    std::uint64_t id_;

    std::string address_;
};

}  // namespace qdb::server

#endif  // QUASARDB_ROUTER_H
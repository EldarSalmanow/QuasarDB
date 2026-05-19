#ifndef QUASARDB_CONFIG_H
#define QUASARDB_CONFIG_H

#include <cstdint>
#include <string>


namespace qdb::storage {

class Config {
public:
    Config(std::string host, std::uint32_t port, std::string root);

public:
    static auto New(std::string host, std::uint32_t port, std::string root) -> Config;

    static auto FromArguments(int argc, char **argv) -> Config;

public:
    auto Host() const -> const std::string &;

    auto Port() const -> std::uint32_t;

    auto Root() const -> const std::string &;

private:
    std::string host_;

    std::uint32_t port_;

    std::string root_;
};

}

#endif  // QUASARDB_CONFIG_H

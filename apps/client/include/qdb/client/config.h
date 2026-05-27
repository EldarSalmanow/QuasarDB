#ifndef QUASARDB_CONFIG_H
#define QUASARDB_CONFIG_H

#include <cstdint>
#include <optional>
#include <string>


namespace qdb::client {

class Config {
public:
    Config(std::string host, std::uint32_t port, std::string file = "");

public:
    static auto New(std::string host, std::uint32_t port, std::string file = "") -> Config;

    static auto FromArguments(int argc, char** argv) -> std::optional<Config>;

public:
    auto Host() const -> const std::string&;

    auto Port() const -> std::uint32_t;

    auto File() const -> const std::string&;

private:
    std::string host_;

    std::uint32_t port_;

    std::string file_;
};

}  // namespace qdb::client

#endif  // QUASARDB_CONFIG_H

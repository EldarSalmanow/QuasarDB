#ifndef QUASARDB_CONFIG_H
#define QUASARDB_CONFIG_H

#include <memory>
#include <string>

namespace qdb::client {

    class Config {
    public:
        static auto Load(const std::string &path) -> std::unique_ptr<Config>;

    public:
        auto Host() const -> const std::string &;

        auto Port() const -> int;

        auto User() const -> const std::string &;

        auto Database() const -> const std::string &;
    };

} // namespace qdb

#endif //QUASARDB_CONFIG_H

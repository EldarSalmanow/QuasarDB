#ifndef QUASARDB_JSON_FILE_H
#define QUASARDB_JSON_FILE_H

#include <nlohmann/json.hpp>

#include <filesystem>

namespace qdb::core {

class JsonFile final {
public:
    explicit JsonFile(std::filesystem::path path);

    auto Load(nlohmann::json fallback = nlohmann::json::object()) const -> nlohmann::json;
    auto Save(const nlohmann::json& data) const -> bool;

private:
    std::filesystem::path path_;
};

}  // namespace qdb::core

#endif  // QUASARDB_JSON_FILE_H

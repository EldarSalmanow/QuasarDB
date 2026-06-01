#include <qdb/core/json_file.h>

#include <fstream>
#include <utility>

namespace qdb::core {

JsonFile::JsonFile(std::filesystem::path path) : path_(std::move(path)) {}

auto JsonFile::Load(nlohmann::json fallback) const -> nlohmann::json {
    std::ifstream input(path_);
    if (!input.is_open()) {
        return fallback;
    }

    auto data = nlohmann::json::parse(input, nullptr, false);
    return data.is_discarded() ? std::move(fallback) : data;
}

auto JsonFile::Save(const nlohmann::json& data) const -> bool {
    if (!path_.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(path_.parent_path(), ec);
        if (ec) {
            return false;
        }
    }

    auto tmp = path_;
    tmp += ".tmp";
    {
        std::ofstream output(tmp, std::ios::trunc);
        if (!output.is_open()) {
            return false;
        }
        output << data.dump(2);
        if (!output.good()) {
            std::error_code ignored;
            std::filesystem::remove(tmp, ignored);
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::permissions(
        tmp,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace,
        ec
    );
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }

    std::filesystem::rename(tmp, path_, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

}  // namespace qdb::core

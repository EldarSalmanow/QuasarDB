#ifndef QUASARDB_JOURNAL_H
#define QUASARDB_JOURNAL_H

#include "record.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace qdb::storage {
class Journal final {
public:
    class Track {
    public:
        enum class Type : uint8_t {
            DELETE,
            UPDATE,
            INSERT,
        };

        static constexpr uint32_t TIME_LEN = 23;

        static std::string get_now();
    };

    struct RevertResult {
        std::string time;
        Track::Type type;
        Record record;
    };

    static constexpr std::string_view JOURNAL_EXT = ".jnl";
    static constexpr std::string_view INDEX_EXT = ".idx";

private:
    fs::path _root;
    fs::path _path;
    std::fstream _file;
    std::streampos _pos;

public:
    Journal(fs::path root, const std::string& name);

    Journal(Journal&&) noexcept = default;
    Journal& operator=(Journal&&) noexcept = default;

    ~Journal() noexcept;

    void close() noexcept;

    void drop();

    std::string save_insertion(const Record& record);

    std::string save_updation(const Record& old_record);

    std::string save_deletion(const Record& record);

    RevertResult revert_last(const std::string& time, const Schema& schema, Interner& interner);

private:
    std::string write_track(Track::Type type, uint32_t record_id, std::vector<uint8_t> data = {});
    void sync();

    void truncate_to_last();
};

}  // namespace qdb::storage

#endif  // QUASARDB_JOURNAL_H

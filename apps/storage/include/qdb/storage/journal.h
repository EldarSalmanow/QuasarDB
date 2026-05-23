#ifndef QUASARDB_JOURNAL_H
#define QUASARDB_JOURNAL_H

#include "record.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>


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

    protected:
        Type _type;
        uint32_t _record_id;
        std::string _time;

        Track(Type type, uint32_t record_id);

    public:
        static std::string get_now();

        Type type() const;

        std::string time() const;

        virtual ~Track() noexcept = default;

        virtual uint32_t write(std::fstream& os);
    };

    class DeleteTrack : public Track {
    public:
        DeleteTrack(uint32_t record_id);

        uint32_t write(std::fstream& os) override;
    };

    class InsertTrack : public Track {
        std::vector<uint8_t> _serialized_record;

    public:
        InsertTrack(const Record& record, const Schema& schema, Serializer* serializer);

        uint32_t write(std::fstream& os) override;
    };

    class UpdateTrack : public Track {
        std::vector<uint8_t> _serialized_record;

    public:
        UpdateTrack(const Record& record, const Schema& schema, Serializer* serializer);

        uint32_t write(std::fstream& os) override;
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

    std::string save_updation(const Record& old_record, const Schema& schema, Serializer* serializer);

    std::string save_deletion(const Record& record, const Schema& schema, Serializer* serializer);

    RevertResult revert_last(
        const std::string& time,
        const Schema& schema,
        StringStorage* str_storage,
        Serializer* serializer
    );

private:
    void truncate_to_last();
};

}  // namespace qdb::storage

#endif  // QUASARDB_JOURNAL_H
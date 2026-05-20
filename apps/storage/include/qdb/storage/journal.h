#ifndef QUASARDB_JOURNAL_H
#define QUASARDB_JOURNAL_H

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "record.h"

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

        Track(Type type, uint32_t record_id) : _type(type), _record_id(record_id), _time(get_now()) {}

    public:
        static std::string get_now() {
            auto now = std::chrono::system_clock::now();
            auto now_ms = std::chrono::floor<std::chrono::milliseconds>(now);
            auto ms = now_ms.time_since_epoch() % std::chrono::seconds(1);
            auto time_t_zone = std::chrono::system_clock::to_time_t(now);
            std::tm tm_zone = *std::localtime(&time_t_zone);
            std::ostringstream oss;
            oss << std::put_time(&tm_zone, "%Y.%m.%d-%H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
                << ms.count();
            return oss.str();
        }

        Type type() const { return _type; }

        std::string time() const { return _time; }

        virtual ~Track() noexcept = default;

        virtual uint32_t write(std::fstream& os) {
            os.write(_time.data(), TIME_LEN);
            os.write(reinterpret_cast<const char*>(&_record_id), sizeof(_record_id));
            os.write(reinterpret_cast<const char*>(&_type), sizeof(_type));
            return TIME_LEN + sizeof(_record_id) + sizeof(_type);
        }
    };

    class DeleteTrack : public Track {
    public:
        DeleteTrack(uint32_t record_id) : Track(Track::Type::DELETE, record_id) {}

        uint32_t write(std::fstream& os) override {
            uint32_t size = Track::write(os) + sizeof(uint32_t);
            os.write(reinterpret_cast<const char*>(&size), sizeof(size));
            return size;
        }
    };

    class InsertTrack : public Track {
        std::vector<uint8_t> _serialized_record;

    public:
        InsertTrack(const Record& record, const Schema& schema, Serializer* serializer)
            : Track(Track::Type::INSERT, record.id()), _serialized_record(record.serialized(schema, serializer)) {}

        uint32_t write(std::fstream& os) override {
            uint32_t size = Track::write(os) + sizeof(uint32_t) + _serialized_record.size() + sizeof(uint32_t);
            uint32_t data_size = static_cast<uint32_t>(_serialized_record.size());
            os.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
            os.write(reinterpret_cast<const char*>(_serialized_record.data()), data_size);
            os.write(reinterpret_cast<const char*>(&size), sizeof(size));
            return size;
        }
    };

    class UpdateTrack : public Track {
        std::vector<uint8_t> _serialized_record;

    public:
        UpdateTrack(const Record& record, const Schema& schema, Serializer* serializer)
            : Track(Track::Type::UPDATE, record.id()), _serialized_record(record.serialized(schema, serializer)) {}

        uint32_t write(std::fstream& os) override {
            uint32_t size = Track::write(os) + sizeof(uint32_t) + _serialized_record.size() + sizeof(uint32_t);
            uint32_t data_size = static_cast<uint32_t>(_serialized_record.size());
            os.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
            os.write(reinterpret_cast<const char*>(_serialized_record.data()), data_size);
            os.write(reinterpret_cast<const char*>(&size), sizeof(size));
            return size;
        }
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
    Journal(fs::path root, const std::string& name)
        : _root(std::move(root)), _path(_root / (name + std::string(JOURNAL_EXT))) {
        _file.open(_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!_file.is_open()) {
            _file.clear();
            _file.open(_path, std::ios::out | std::ios::binary | std::ios::trunc);
            _file.close();
            _file.open(_path, std::ios::in | std::ios::out | std::ios::binary);
        }
        if (!_file.is_open()) {
            throw std::runtime_error("Journal cannot open file " + std::string(_path) + ".");
        }
        _pos = _file.seekp(0, std::ios::end).tellp();
    }

    Journal(Journal&&) noexcept = default;
    Journal& operator=(Journal&&) noexcept = default;

    ~Journal() noexcept { close(); }

    void close() noexcept {
        if (_file.is_open()) {
            _file.seekp(0, std::ios::end);
            if (_pos != _file.tellp()) {
                truncate_to_last();
            }
            _file.close();
        }
    }

    void drop() {
        close();
        fs::remove(_path);
    }

    std::string save_insertion(const Record& record) {
        _file.seekp(0, std::ios::end);
        if (_pos != _file.tellp()) {
            truncate_to_last();
        }
        DeleteTrack track(record.id());
        track.write(_file);
        _pos = _file.seekp(0, std::ios::end).tellp();
        _file.flush();
        return track.time();
    }

    std::string save_updation(const Record& old_record, const Schema& schema, Serializer* serializer) {
        _file.seekp(0, std::ios::end);
        if (_pos != _file.tellp()) {
            truncate_to_last();
        }
        UpdateTrack track(old_record, schema, serializer);
        track.write(_file);
        _pos = _file.seekp(0, std::ios::end).tellp();
        _file.flush();
        return track.time();
    }

    std::string save_deletion(const Record& record, const Schema& schema, Serializer* serializer) {
        _file.seekp(0, std::ios::end);
        if (_pos != _file.tellp()) {
            truncate_to_last();
        }
        InsertTrack track(record, schema, serializer);
        track.write(_file);
        _pos = _file.seekp(0, std::ios::end).tellp();
        _file.flush();
        return track.time();
    }

    RevertResult revert_last(
        const std::string& time,
        const Schema& schema,
        StringStorage* str_storage,
        Serializer* serializer
    ) {
        if (_pos <= 0) return {"", Track::Type::INSERT, Record(0, 1)};
        auto last_pos = _pos;
        _file.seekg(_pos - static_cast<std::streamoff>(sizeof(uint32_t)), std::ios::beg);
        uint32_t track_size;
        _file.read(reinterpret_cast<char*>(&track_size), sizeof(track_size));
        _file.seekg(_pos - static_cast<std::streamoff>(track_size), std::ios::beg);
        _pos = _file.tellg();

        std::string writed_time(Track::TIME_LEN, '\0');
        _file.read(&writed_time[0], Track::TIME_LEN);
        std::cout << "revert_last: time: " << time << ", writed_time: " << writed_time << std::endl;
        if (writed_time < time) {
            _pos = last_pos;
            return {"", Track::Type::INSERT, Record(0, 1)};
        }
        uint32_t record_id;
        Track::Type type;
        _file.read(reinterpret_cast<char*>(&record_id), sizeof(record_id));
        _file.read(reinterpret_cast<char*>(&type), sizeof(type));
        Record record(record_id, 1);
        if (type == Track::Type::INSERT || type == Track::Type::UPDATE) {
            uint32_t data_size;
            _file.read(reinterpret_cast<char*>(&data_size), sizeof(data_size));
            std::vector<uint8_t> data(data_size);
            _file.read(reinterpret_cast<char*>(data.data()), data_size);
            record = Record::from_binary(data.data(), data_size, record_id, schema, str_storage, serializer);
        }
        return {writed_time, type, std::move(record)};
    }

private:
    void truncate_to_last() {
        if (_file.is_open()) {
            _file.close();
        }
        std::error_code ec;
        std::filesystem::resize_file(_path, static_cast<std::streamoff>(_pos), ec);
        if (ec) {
            throw std::runtime_error("Failed to truncate journal: " + ec.message());
        }
        _file.open(_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!_file.is_open()) {
            throw std::runtime_error("Journal cannot reopen file after truncation.");
        }
        _file.seekp(_pos, std::ios::beg);
        _file.seekg(_pos, std::ios::beg);
    }
};

}  // namespace qdb::storage

#endif  // QUASARDB_JOURNAL_H
#include "../include/qdb/storage/journal.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace qdb::storage {

std::string Journal::Track::get_now() {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::floor<std::chrono::milliseconds>(now);
    auto ms = now_ms.time_since_epoch() % std::chrono::seconds(1);
    auto time_t_zone = std::chrono::system_clock::to_time_t(now);
    std::tm tm_zone = *std::localtime(&time_t_zone);
    std::ostringstream oss;
    oss << std::put_time(&tm_zone, "%Y.%m.%d-%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

Journal::Journal(fs::path root, const std::string& name)
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

Journal::~Journal() noexcept { close(); }

void Journal::close() noexcept {
    if (_file.is_open()) {
        _file.seekp(0, std::ios::end);
        if (_pos != _file.tellp()) {
            truncate_to_last();
        }
        _file.close();
    }
}

void Journal::drop() {
    close();
    fs::remove(_path);
}

std::string Journal::save_insertion(const Record& record) {
    return write_track(Track::Type::DELETE, record.Id());
}

std::string Journal::save_updation(const Record& old_record) {
    return write_track(Track::Type::UPDATE, old_record.Id(), old_record.Serialize());
}

std::string Journal::save_deletion(const Record& record) {
    return write_track(Track::Type::INSERT, record.Id(), record.Serialize());
}

std::string Journal::write_track(Track::Type type, uint32_t record_id, std::vector<uint8_t> data) {
    _file.seekp(0, std::ios::end);
    if (_pos != _file.tellp()) {
        truncate_to_last();
    }

    auto time = Track::get_now();
    auto size = static_cast<uint32_t>(Track::TIME_LEN + sizeof(record_id) + sizeof(type) + sizeof(uint32_t));
    if (!data.empty()) {
        size += static_cast<uint32_t>(sizeof(uint32_t) + data.size());
    }

    _file.write(time.data(), Track::TIME_LEN);
    _file.write(reinterpret_cast<const char*>(&record_id), sizeof(record_id));
    _file.write(reinterpret_cast<const char*>(&type), sizeof(type));
    if (!data.empty()) {
        auto data_size = static_cast<uint32_t>(data.size());
        _file.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
        _file.write(reinterpret_cast<const char*>(data.data()), data_size);
    }
    _file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    _pos = _file.seekp(0, std::ios::end).tellp();
    _file.flush();
    return time;
}

Journal::RevertResult Journal::revert_last(const std::string& time, const Schema& schema, Interner& interner) {
    if (_pos <= 0) return {"", Track::Type::INSERT, Record(0, 1)};
    auto last_pos = _pos;
    _file.seekg(_pos - static_cast<std::streamoff>(sizeof(uint32_t)), std::ios::beg);
    uint32_t track_size;
    _file.read(reinterpret_cast<char*>(&track_size), sizeof(track_size));
    _file.seekg(_pos - static_cast<std::streamoff>(track_size), std::ios::beg);
    _pos = _file.tellg();

    std::string writed_time(Track::TIME_LEN, '\0');
    _file.read(&writed_time[0], Track::TIME_LEN);
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
        record = Record::FromBinary(data.data(), data_size, record_id, schema, interner);
    }
    return {writed_time, type, std::move(record)};
}

void Journal::truncate_to_last() {
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
}  // namespace qdb::storage

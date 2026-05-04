#ifndef QUASARDB_STRING_STORAGE_H
#define QUASARDB_STRING_STORAGE_H

#include <deque>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include "external_string.h"

namespace qdb::storage {

namespace fs = std::filesystem;

class StringStorage {
    fs::path _data_path;
    std::fstream _file;

public:
    StringStorage(fs::path path) : _data_path(std::move(path)) {
        _file.open(_data_path, std::ios::in | std::ios::out | std::ios::binary);
        if (!_file.is_open()) {
            _file.open(_data_path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!_file.is_open()) {
                throw std::runtime_error("Cannot create file: " + _data_path.string());
            }
            _file.close();
            _file.open(_data_path, std::ios::in | std::ios::out | std::ios::binary);
        }
        if (!_file.is_open()) {
            throw std::runtime_error("Cannot open file: " + _data_path.string() + ".");
        }
    }

    StringStorage(const StringStorage&) = delete;
    StringStorage& operator=(const StringStorage&) = delete;

    StringStorage(StringStorage&& other) noexcept : _data_path(other._data_path), _file(std::move(other._file)) {}

    StringStorage& operator=(StringStorage&& other) noexcept {
        if (this != &other) {
            _data_path = other._data_path;
            _file = std::move(other._file);
        }
        return *this;
    }

    ~StringStorage() noexcept { close(); }

    void close() noexcept {
        if (_file.is_open()) {
            _file.close();
        }
    }

    void delete_file() {
        close();
        fs::remove(_data_path);
    }

    std::string read(const ExternalString str_addr) {
        _file.clear();
        _file.seekg(str_addr.offset);
        uint64_t str_size;
        if (!_file.read(reinterpret_cast<char*>(&str_size), sizeof(str_size))) {
            throw std::runtime_error("Failed to read string size.");
        }
        if (str_size != str_addr.size) {
            throw std::runtime_error(
                "String size mismatch: get_size=" + std::to_string(str_addr.size) +
                ", disk_size=" + std::to_string(str_size) + "."
            );
        }
        std::string result(str_addr.size, '\0');
        if (!_file.read(result.data(), str_size)) {
            throw std::runtime_error(
                "Failed to read complete string (offset=" + std::to_string(str_addr.offset) +
                ", size=" + std::to_string(str_addr.size) + "."
            );
        }
        return result;
    }

    ExternalString append(std::string_view str) {
        _file.clear();
        _file.seekp(0, std::ios::end);
        ExternalString result = {.offset = static_cast<uint64_t>(_file.tellp()), .size = str.size()};
        uint64_t str_size = str.size();
        if (!_file.write(reinterpret_cast<const char*>(&str_size), sizeof(str_size)) ||
            !_file.write(str.data(), str.size())) {
            throw std::runtime_error("Failed to write string data.");
        }
        _file.flush();
        if (_file.fail()) {
            throw std::runtime_error("Cannot append string to file.");
        }
        return result;
    }
};

}  // namespace qdb::storage

#endif  // QUASARDB_STRING_STORAGE_H
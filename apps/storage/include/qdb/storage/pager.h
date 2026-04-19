//
// Created by eldar on 09.04.2026.
//

#ifndef QUASARDB_PAGER_H
#define QUASARDB_PAGER_H

#include <string>
#include <fstream>

class Pager final {

    size_t page_size;
    std::string file_path;
    std::fstream db_file;

public:

    ~Pager() {
        if (db_file.is_open()) {
            db_file.close();
        }
    }

    Pager(const std::string& path, size_t page_size)
        : page_size(page_size),
          file_path(path)
    {
        db_file.open(path, std::ios::in | std::ios::out | std::ios::binary);
        if (!db_file.is_open()) {
            db_file.clear();
            db_file.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
            db_file.close();
            db_file.open(path, std::ios::in | std::ios::out | std::ios::binary);
        }
        if (!db_file.is_open()) {
            throw std::runtime_error("Pager cannot open file " + path + ".");
        }
    }

    uint32_t get_total_pages() {
        db_file.clear();
        std::streampos current_pos = db_file.tellg();
        db_file.seekg(0, std::ios::end);
        std::streampos file_size = db_file.tellg();
        db_file.seekg(current_pos);
        return static_cast<uint32_t>(file_size / page_size);
    }

    bool page_exists(uint32_t page_id) {
        return page_id < get_total_pages();
    }

    void write_page(uint32_t page_id, std::vector<uint8_t>& page_data) {
        if (page_data.size() != page_size) {
            throw std::runtime_error("Write page error: page_data.size() != page_size");
        }
        db_file.seekp(page_id * page_size);
        db_file.write(reinterpret_cast<const char*>(page_data.data()), page_size);
        db_file.flush();
    }

    void read_page(uint32_t page_id, std::vector<uint8_t>& page_data) {
        db_file.seekg(page_id * page_size);
        page_data.resize(page_size);
        db_file.read(reinterpret_cast<char*>(page_data.data()), page_size);
        if (static_cast<size_t>(db_file.gcount()) != page_size) {
            throw std::runtime_error("Failed to read complete page " + std::to_string(page_id));
        }
    }

    uint32_t append_new_page() {
        db_file.seekg(0, std::ios::end);
        uint32_t page_id = static_cast<uint32_t>(db_file.tellg() / page_size);
        db_file.seekp(0, std::ios::end);
        db_file.write(reinterpret_cast<const char*>(std::vector<uint8_t>(page_size, 0).data()), page_size);
        db_file.flush();
        return page_id;
    }

};

#endif  // QUASARDB_PAGER_H
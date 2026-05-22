#ifndef QUASARDB_PAGER_H
#define QUASARDB_PAGER_H

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace qdb::storage {

class Pager final {
    size_t page_size;
    std::string file_path;
    std::fstream db_file;

    std::vector<uint8_t> white_page;

public:
    Pager(Pager&& other) noexcept;

    Pager& operator=(Pager&& other) noexcept;

    Pager(const Pager&) = delete;
    Pager& operator=(const Pager&) = delete;

    ~Pager() noexcept;

    Pager(const std::string& path, size_t page_size);

    void close() noexcept;

    uint32_t get_total_pages();

    bool page_exists(uint32_t page_id);

    void write_page(uint32_t page_id, const uint8_t* page_data);

    void read_page(uint32_t page_id, uint8_t* page_data);

    uint32_t append_new_page();

    void truncate(uint32_t count);
};

}  // namespace qdb::storage

#endif  // QUASARDB_PAGER_H
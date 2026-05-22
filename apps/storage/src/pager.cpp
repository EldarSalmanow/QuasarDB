#include "../include/qdb/storage/pager.h"

namespace qdb::storage {

Pager::Pager(Pager&& other) noexcept
    : page_size(other.page_size),
      file_path(std::move(other.file_path)),
      db_file(std::move(other.db_file)),
      white_page(std::move(other.white_page)) {}

Pager& Pager::operator=(Pager&& other) noexcept {
    if (this != &other) {
        page_size = other.page_size;
        file_path = std::move(other.file_path);
        db_file = std::move(other.db_file);
        white_page = std::move(other.white_page);
    }
    return *this;
}

Pager::~Pager() noexcept { close(); }

Pager::Pager(const std::string& path, size_t page_size) : page_size(page_size), file_path(path) {
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
    white_page.assign(page_size, 0);
}

void Pager::close() noexcept {
    if (db_file.is_open()) {
        db_file.close();
    }
}

uint32_t Pager::get_total_pages() {
    db_file.clear();
    std::streampos current_pos = db_file.tellg();
    db_file.seekg(0, std::ios::end);
    std::streampos file_size = db_file.tellg();
    db_file.seekg(current_pos);
    assert(file_size % page_size == 0);
    return static_cast<uint32_t>(file_size / page_size);
}

bool Pager::page_exists(uint32_t page_id) { return page_id < get_total_pages(); }

void Pager::write_page(uint32_t page_id, const uint8_t* page_data) {
    db_file.clear();
    db_file.seekp(page_id * page_size);
    db_file.write(reinterpret_cast<const char*>(page_data), page_size);
    db_file.flush();
}

void Pager::read_page(uint32_t page_id, uint8_t* page_data) {
    db_file.clear();
    db_file.seekg(page_id * page_size);
    db_file.read(reinterpret_cast<char*>(page_data), page_size);
    if (static_cast<size_t>(db_file.gcount()) != page_size) {
        throw std::runtime_error("Failed to read complete page " + std::to_string(page_id));
    }
}

uint32_t Pager::append_new_page() {
    db_file.clear();
    db_file.seekg(0, std::ios::end);
    uint32_t page_id = static_cast<uint32_t>(db_file.tellg() / page_size);
    db_file.seekp(0, std::ios::end);
    db_file.write(reinterpret_cast<const char*>(white_page.data()), page_size);
    db_file.flush();
    return page_id;
}

void Pager::truncate(uint32_t count) {
    db_file.close();
    std::filesystem::resize_file(file_path, count * page_size);
    db_file.open(file_path, std::ios::in | std::ios::out | std::ios::binary);
}

}  // namespace qdb::storage

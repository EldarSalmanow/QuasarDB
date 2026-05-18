#include <qdb/client/reader.h>

#include <iostream>


namespace qdb::client {

IReader::~IReader() = default;

ConsoleReader::ConsoleReader() = default;

std::optional<std::string> ConsoleReader::ReadCommand() {
    std::string line;

    std::cout << "qdb> " << std::flush;
    if (!std::getline(std::cin, line)) {
        return std::nullopt;
    }

    size_t start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return std::string("");
    }

    size_t end = line.find_last_not_of(" \t\r\n");

    return line.substr(start, end - start + 1);
}

bool ConsoleReader::HasMore() const {
    return !std::cin.eof();
}

FileReader::FileReader(const std::string& file_path)
        : file_path_(file_path) {
    file_stream_.open(file_path_);
}

FileReader::~FileReader() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

std::optional<std::string> FileReader::ReadCommand() {
    if (!file_stream_.is_open()) {
        return std::nullopt;
    }

    std::string line;

    while (std::getline(file_stream_, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");

        if (start != std::string::npos) {
            size_t end = line.find_last_not_of(" \t\r\n");

            return line.substr(start, end - start + 1);
        }
    }

    return std::nullopt;
}

bool FileReader::HasMore() const {
    if (!file_stream_.is_open()) {
        return false;
    }

    auto &stream = const_cast<std::ifstream &>(file_stream_);
    const std::streampos current_pos = stream.tellg();
    const std::ios::iostate current_state = stream.rdstate();

    stream.clear();

    std::string line;
    bool has_more = false;
    while (std::getline(stream, line)) {
        const size_t start = line.find_first_not_of(" \t\r\n");

        if (start != std::string::npos) {
            has_more = true;

            break;
        }
    }

    stream.clear();

    if (current_pos != std::streampos(-1)) {
        stream.seekg(current_pos);
    }

    stream.setstate(current_state);

    return has_more;
}

}  // namespace qdb::client

#include <qdb/client/reader.h>

#include <iostream>
#include <string_view>


namespace qdb::client {

namespace {

auto Trim(std::string_view value) -> std::string {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(start, end - start + 1));
}

auto EndsStatement(std::string_view value) -> bool {
    bool in_string = false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '"' && (i == 0 || value[i - 1] != '\\')) {
            in_string = !in_string;
        }
        if (!in_string && value[i] == ';') {
            return true;
        }
    }
    return false;
}

}  // namespace

IReader::~IReader() = default;

ConsoleReader::ConsoleReader() = default;

std::optional<std::string> ConsoleReader::ReadCommand() {
    std::string line;
    std::string command;

    while (true) {
        std::cout << (command.empty() ? "qdb> " : "...> ") << std::flush;
        if (!std::getline(std::cin, line)) {
            return command.empty() ? std::nullopt : std::optional<std::string>(Trim(command));
        }

        if (Trim(line).empty() && command.empty()) {
            return std::string("");
        }

        if (!command.empty()) {
            command += '\n';
        }
        command += line;

        if (EndsStatement(command)) {
            return Trim(command);
        }
    }
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
    std::string command;

    while (std::getline(file_stream_, line)) {
        if (Trim(line).empty() && command.empty()) {
            continue;
        }
        if (!command.empty()) {
            command += '\n';
        }
        command += line;
        if (EndsStatement(command)) {
            return Trim(command);
        }
    }

    const auto trimmed = Trim(command);
    return trimmed.empty() ? std::nullopt : std::optional<std::string>(trimmed);
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

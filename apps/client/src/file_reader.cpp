#include <qdb/client/reader.h>

namespace {

std::string Trim(const std::string& input) {
    const size_t start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const size_t end = input.find_last_not_of(" \t\r\n");
    return input.substr(start, end - start + 1);
}

}

namespace qdb::client {

FileReader::FileReader(const std::string& file_path) : file_path_(file_path) {
    file_stream_.open(file_path_);
    if (file_stream_.is_open()) {
        is_open_ = true;
    }
}

std::optional<std::string> FileReader::ReadCommand() {
    if (!is_open_ || !file_stream_.is_open()) {
        return std::nullopt;
    }

    std::string command;
    std::string line;

    while (std::getline(file_stream_, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty()) {
            continue;
        }

        if (trimmed.back() == ';') {
            trimmed.pop_back();
            trimmed = Trim(trimmed);
            if (!trimmed.empty()) {
                if (!command.empty()) {
                    command += " ";
                }
                command += trimmed;
            }

            const std::string result = Trim(command);
            if (!result.empty()) {
                return result;
            }

            command.clear();
            continue;
        }

        if (!command.empty()) {
            command += " ";
        }
        command += trimmed;
    }

    const std::string result = Trim(command);
    if (!result.empty()) {
        return result;
    }

    return std::nullopt;
}

bool FileReader::HasMore() const {
    if (!is_open_ || !file_stream_.is_open()) {
        return false;
    }

    auto& stream = const_cast<std::ifstream&>(file_stream_);
    const std::streampos current_pos = stream.tellg();
    const std::ios::iostate current_state = stream.rdstate();

    stream.clear();

    std::string line;
    bool has_more = false;
    while (std::getline(stream, line)) {
        const std::string trimmed = Trim(line);
        if (!trimmed.empty() && trimmed != ";") {
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

}

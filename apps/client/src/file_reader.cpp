#include <qdb/client/reader.h>

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
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        
        if (start == std::string::npos) {
            continue;
        }

        std::string trimmed = line.substr(start, end - start + 1);

        if (!trimmed.empty() && trimmed.back() == ';') {
            trimmed.pop_back();
            if (!command.empty()) {
                command += " ";
            }
            command += trimmed;
            
            start = command.find_first_not_of(" \t\r\n");
            end = command.find_last_not_of(" \t\r\n");
            if (start != std::string::npos) {
                return command.substr(start, end - start + 1);
            }
            return command;
        }

        if (!command.empty()) {
            command += " ";
        }
        command += trimmed;
    }

    if (!command.empty()) {
        return command;
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

}

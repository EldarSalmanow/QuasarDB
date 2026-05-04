#include <qdb/client/reader.h>

#include <iostream>

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

std::optional<std::string> ConsoleReader::ReadCommand() {
    std::string command;
    std::string line;
    bool first_line = true;

    while (true) {
        if (first_line) {
            std::cout << "quasar> " << std::flush;
        } else {
            std::cout << "      -> " << std::flush;
        }

        if (!std::getline(std::cin, line)) {
            const std::string result = Trim(command);
            if (!result.empty()) {
                return result;
            }
            return std::nullopt;
        }

        std::string trimmed = Trim(line);
        if (trimmed.empty()) {
            first_line = command.empty();
            continue;
        }

        if (!trimmed.empty() && trimmed.back() == ';') {
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
            first_line = true;
            continue;
        }

        if (!command.empty()) {
            command += " ";
        }
        command += trimmed;
        first_line = false;
    }
}

bool ConsoleReader::HasMore() const {
    return !std::cin.eof();
}

}

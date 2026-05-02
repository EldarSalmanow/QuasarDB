#include <qdb/client/reader.h>

#include <iostream>
#include <sstream>

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
            if (!command.empty()) {
                return command;
            }
            return std::nullopt;
        }

        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        
        if (start == std::string::npos) {
            first_line = false;
            continue;
        }

        std::string trimmed = line.substr(start, end - start + 1);

        if (!trimmed.empty() && trimmed.back() == ';') {
            trimmed.pop_back();
            if (!trimmed.empty()) {
                if (!command.empty()) {
                    command += " ";
                }
                command += trimmed;
            }

            start = command.find_first_not_of(" \t\r\n");
            end = command.find_last_not_of(" \t\r\n");
            if (start != std::string::npos) {
                return command.substr(start, end - start + 1);
            }
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

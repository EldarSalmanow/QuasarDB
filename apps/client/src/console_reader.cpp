#include <qdb/client/reader.h>

#include <iostream>
#include <sstream>

namespace qdb::client {

std::optional<std::string> ConsoleReader::readCommand() {
    std::string command;
    std::string line;
    bool first_line = true;

    while (true) {
        // Display prompt
        if (first_line) {
            std::cout << "quasar> " << std::flush;
        } else {
            std::cout << "      -> " << std::flush;
        }

        // Read line from stdin
        if (!std::getline(std::cin, line)) {
            // EOF or error
            if (!command.empty()) {
                // Return accumulated command even without semicolon
                return command;
            }
            return std::nullopt;
        }

        // Trim whitespace from line
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        
        if (start == std::string::npos) {
            // Empty line, continue reading
            first_line = false;
            continue;
        }

        std::string trimmed = line.substr(start, end - start + 1);

        // Check if line ends with semicolon
        if (!trimmed.empty() && trimmed.back() == ';') {
            // Remove semicolon and add to command
            trimmed.pop_back();
            if (!command.empty()) {
                command += " ";
            }
            command += trimmed;
            
            // Trim final command
            start = command.find_first_not_of(" \t\r\n");
            end = command.find_last_not_of(" \t\r\n");
            if (start != std::string::npos) {
                return command.substr(start, end - start + 1);
            }
            return command;
        }

        // Add line to command and continue
        if (!command.empty()) {
            command += " ";
        }
        command += trimmed;
        first_line = false;
    }
}

bool ConsoleReader::hasMore() const {
    return !std::cin.eof();
}

}  // namespace qdb::client

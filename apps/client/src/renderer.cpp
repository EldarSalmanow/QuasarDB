#include <qdb/client/renderer.h>

#include <qdb/core/response.h>

#include <nlohmann/json.hpp>

#include <iomanip>
#include <iostream>
#include <vector>

namespace qdb::client {

const std::string RESET = "\033[0m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string CYAN = "\033[36m";

void Renderer::RenderWelcome() const {
    std::cout << CYAN << "QuasarDB Client v1.0" << RESET << std::endl;
    std::cout << "Type your SQL queries and press Enter. End with ';'" << std::endl;
    std::cout << std::endl;
}

void Renderer::RenderResponse(const qdb::core::Response& response) const {
    if (response.IsError()) {
        std::cout << RED << "Error: " << response.GetMessage() << RESET << std::endl;

        return;
    }

    std::cout << GREEN << response.GetMessage() << RESET << std::endl;

    const std::string& data = response.GetData();
    if (!data.empty() && data != "\"\"") {
        RenderTable(data);
    }
}

void Renderer::RenderError(const std::string& message) const {
    std::cout << RED << "Error: " << message << RESET << std::endl;
}

void Renderer::RenderAsyncSubmitted(const std::string& task_id) const {
    std::cout << CYAN << "Async query submitted" << RESET << std::endl;
    std::cout << "Task ID: " << task_id << std::endl;
    std::cout << "Polling for completion..." << std::endl;
}

void Renderer::RenderPollingProgress(char spinner_char, std::size_t attempt) const {
    std::cout << "\r  [" << spinner_char << "] Waiting... (attempt " << attempt << ")" << std::flush;
}

void Renderer::RenderTable(const std::string& json_data) const {
    try {
        nlohmann::json parsed;

        try {
            parsed = nlohmann::json::parse(json_data);
        } catch (...) {
            std::cout << json_data << std::endl;

            return;
        }

        if (parsed.is_array() && !parsed.empty()) {
            auto first = parsed[0];

            if (first.is_object()) {
                std::vector<std::string> columns;
                for (auto it = first.begin(); it != first.end(); ++it) {
                    columns.push_back(it.key());
                }

                std::vector<size_t> widths(columns.size(), 0);
                for (size_t i = 0; i < columns.size(); ++i) {
                    widths[i] = columns[i].length();
                }

                for (const auto& row : parsed) {
                    for (size_t i = 0; i < columns.size(); ++i) {
                        std::string value = row[columns[i]].is_null() ? "NULL" : row[columns[i]].dump();

                        if (value.front() == '"' && value.back() == '"') {
                            value = value.substr(1, value.length() - 2);
                        }

                        widths[i] = std::max(widths[i], value.length());
                    }
                }

                std::cout << std::endl;
                for (size_t i = 0; i < columns.size(); ++i) {
                    std::cout << "| " << std::left << std::setw(widths[i]) << columns[i] << " ";
                }
                std::cout << "|" << std::endl;

                for (size_t i = 0; i < columns.size(); ++i) {
                    std::cout << "+-" << std::string(widths[i], '-') << "-";
                }
                std::cout << "+" << std::endl;

                for (const auto& row : parsed) {
                    for (size_t i = 0; i < columns.size(); ++i) {
                        std::string value = row[columns[i]].is_null() ? "NULL" : row[columns[i]].dump();

                        if (value.front() == '"' && value.back() == '"') {
                            value = value.substr(1, value.length() - 2);
                        }

                        std::cout << "| " << std::left << std::setw(widths[i]) << value << " ";
                    }

                    std::cout << "|" << std::endl;
                }

                std::cout << std::endl;

                std::cout << parsed.size() << " row(s) returned" << std::endl;

                return;
            }
        }

        if (parsed.is_object()) {
            std::cout << parsed.dump(2) << std::endl;

            return;
        }

        std::cout << parsed.dump(2) << std::endl;

    } catch (const std::exception& exception) {
        std::cout << "Data: " << json_data << std::endl;
    }
}

}  // namespace qdb::client

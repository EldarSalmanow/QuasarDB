#include <qdb/client/connection.h>
#include <qdb/client/renderer.h>
#include <qdb/client/reader.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

void PrintUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --host <address>  Server address (default: localhost)" << std::endl;
    std::cout << "  -p, --port <port>     Server port (default: 4567)" << std::endl;
    std::cout << "  -f, --file <path>     Execute commands from file" << std::endl;
    std::cout << "  --help                Show this help message" << std::endl;
}

bool ParseArgs(int argc, char** argv, std::string& host, uint32_t& port, std::string& file) {
    host = "localhost";
    port = 4567;
    file = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-?") {
            PrintUsage(argv[0]);
            return false;
        } else if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) {
                host = argv[++i];
            }
        } else if (arg == "-p" || arg == "--port") {
            if (i + 1 < argc) {
                port = static_cast<uint32_t>(std::stoi(argv[++i]));
            }
        } else if (arg == "-f" || arg == "--file") {
            if (i + 1 < argc) {
                file = argv[++i];
            }
        }
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::string host;
    uint32_t port;
    std::string file;

    if (!ParseArgs(argc, argv, host, port, file)) {
        return 0;
    }

    qdb::client::Renderer renderer;
    renderer.RenderWelcome();

    auto connection = std::make_unique<qdb::client::Connection>(host, port);

    if (!connection->Connect()) {
        renderer.RenderError("Failed to connect to server " + host + ":" + std::to_string(port));
        return 1;
    }

    std::unique_ptr<qdb::client::IInputStream> reader;
    if (!file.empty()) {
        reader = std::make_unique<qdb::client::FileReader>(file);
    } else {
        reader = std::make_unique<qdb::client::ConsoleReader>();
    }

    while (reader->HasMore()) {
        auto command = reader->ReadCommand();
        if (!command.has_value()) {
            break;
        }

        if (command->empty()) {
            continue;
        }

        auto response = connection->ExecuteQuery(command.value());
        renderer.RenderResponse(response);
    }

    connection->Disconnect();

    return 0;
}
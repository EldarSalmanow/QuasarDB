#include <qdb/client/application.h>
#include <qdb/client/reader.h>

#include <qdb/core/request.h>

#include <fstream>

namespace qdb::client {

Application::Application(Config config)
    : config_(std::move(config)), client_(qdb::core::TcpClient::New(config_.Host(), config_.Port())) {}

auto Application::New(Config config) -> std::unique_ptr<Application> {
    return std::make_unique<Application>(std::move(config));
}

auto Application::Run() -> std::int32_t {
    renderer_.RenderWelcome();

    if (!client_ || !client_->Connect()) {
        renderer_.RenderError("Failed to connect to server " + config_.Host() + ":" + std::to_string(config_.Port()));

        return 1;
    }

    std::unique_ptr<IReader> reader;

    if (!config_.File().empty()) {
        if (!std::ifstream(config_.File()).is_open()) {
            renderer_.RenderError("Failed to open file: " + config_.File());
            return 1;
        }
        reader = std::make_unique<FileReader>(config_.File());
    } else {
        reader = std::make_unique<ConsoleReader>();
    }

    while (reader->HasMore()) {
        auto command = reader->ReadCommand();

        if (!command.has_value()) {
            break;
        }

        if (command->empty()) {
            continue;
        }

        auto request = qdb::core::RequestBuilder::Query(command.value()).Build();

        if (!client_->SendRequest(request)) {
            renderer_.RenderError("Failed to send request");

            continue;
        }

        auto response = client_->ReceiveResponse();

        if (!response.has_value()) {
            renderer_.RenderError("Failed to receive response");

            continue;
        }

        renderer_.RenderResponse(response.value());
    }

    client_->Disconnect();

    return 0;
}

}  // namespace qdb::client


#include <qdb/client/application.h>
#include <qdb/client/reader.h>

#include <qdb/core/request.h>

#include <chrono>
#include <fstream>
#include <thread>

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

        if (response->IsPending()) {
            HandleAsyncResponse(response.value());
        } else {
            renderer_.RenderResponse(response.value());
        }
    }

    client_->Disconnect();

    return 0;
}

void Application::HandleAsyncResponse(const qdb::core::Response& response) {
    const auto& data = response.GetDataObject();
    if (!data.contains("task_id")) {
        renderer_.RenderError("Server returned pending status without task_id");
        return;
    }

    const auto task_id = data["task_id"].get<std::string>();
    renderer_.RenderAsyncSubmitted(task_id);

    auto result = PollTask(task_id);

    if (!result.has_value()) {
        renderer_.RenderError("Failed to poll task status: " + task_id);
        return;
    }

    renderer_.RenderResponse(result.value());
}

auto Application::PollTask(const std::string& task_id) -> std::optional<qdb::core::Response> {
    constexpr std::size_t max_attempts = 600;
    constexpr auto poll_interval = std::chrono::milliseconds(500);
    constexpr const char spinner[] = {'|', '/', '-', '\\'};
    std::size_t spin_index = 0;

    for (std::size_t attempt = 0; attempt < max_attempts; ++attempt) {
        auto request = qdb::core::RequestBuilder::CheckTask(task_id).Build();

        if (!client_->SendRequest(request)) {
            return std::nullopt;
        }

        auto response = client_->ReceiveResponse();

        if (!response.has_value()) {
            return std::nullopt;
        }

        if (response->IsPending()) {
            renderer_.RenderPollingProgress(spinner[spin_index % 4], attempt + 1);
            ++spin_index;
            std::this_thread::sleep_for(poll_interval);
            continue;
        }

        return response;
    }

    return std::nullopt;
}

}  // namespace qdb::client


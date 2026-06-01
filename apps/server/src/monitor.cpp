#include <qdb/server/monitor.h>

#include <qdb/core/request.h>
#include <qdb/core/tcp_client.h>

#include <utility>

namespace qdb::server {

Monitor::Monitor() : Monitor(Registry::New()) {}

Monitor::Monitor(std::shared_ptr<Registry> registry, std::chrono::milliseconds interval, std::uint32_t dead_threshold)
    : registry_(std::move(registry)), interval_(interval), dead_threshold_(dead_threshold == 0 ? 1 : dead_threshold) {}

Monitor::~Monitor() { Stop(); }

auto Monitor::New() -> std::unique_ptr<Monitor> { return std::make_unique<Monitor>(); }

auto Monitor::New(std::shared_ptr<Registry> registry, std::chrono::milliseconds interval, std::uint32_t dead_threshold)
    -> std::unique_ptr<Monitor> {
    return std::make_unique<Monitor>(std::move(registry), interval, dead_threshold);
}

auto Monitor::Start() -> void {
    if (running_.exchange(true)) {
        return;
    }

    worker_ = std::thread([this] {
        while (running_) {
            ProbeOnce();

            std::this_thread::sleep_for(interval_);
        }
    });
}

auto Monitor::Stop() -> void {
    if (!running_.exchange(false)) {
        return;
    }

    if (worker_.joinable()) {
        worker_.join();
    }
}

auto Monitor::ProbeOnce() -> void {
    if (!registry_) {
        return;
    }

    for (const auto& [id, node] : registry_->GetNodes()) {
        if (PingStorage(node)) {
            missed_heartbeats_.erase(id);
            registry_->UpdateNode(id, StorageState::Up);
            continue;
        }

        auto& missed = missed_heartbeats_[id];
        ++missed;
        if (missed >= dead_threshold_) {
            missed = 0;
            if (!registry_->RestartNode(id)) {
                registry_->UpdateNode(id, StorageState::Down);
            }
        }
    }
}

auto Monitor::PingStorage(const StorageNode& node) -> bool {
    auto client = qdb::core::TcpClient::New(node.host, node.port);
    if (!client || !client->Connect()) {
        return false;
    }

    if (!client->SendRequest(qdb::core::RequestBuilder().Action("ping").Build())) {
        client->Disconnect();
        return false;
    }

    auto response = client->ReceiveResponse();
    client->Disconnect();

    return response.has_value() && response->IsSuccess();
}

}  // namespace qdb::server

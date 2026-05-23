#include <qdb/server/registry.h>

#include <qdb/core/tcp_client.h>

#include <chrono>
#include <cctype>
#include <csignal>
#include <fcntl.h>
#include <filesystem>
#include <mutex>
#include <thread>
#include <utility>
#include <sys/wait.h>
#include <unistd.h>

namespace qdb::server {

namespace {

auto SanitizedPath(std::string value) -> std::filesystem::path {
    std::filesystem::path path;
    std::string part;
    for (auto symbol : value) {
        if (symbol == '.') {
            path /= part.empty() ? "_" : part;
            part.clear();
            continue;
        }
        part += std::isalnum(static_cast<unsigned char>(symbol)) || symbol == '_' || symbol == '-' ? symbol : '_';
    }
    return path / (part.empty() ? "_" : part);
}

auto ExistingExecutable(const std::vector<std::filesystem::path>& candidates) -> std::filesystem::path {
    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && !std::filesystem::is_directory(candidate, ec)) {
            return candidate;
        }
    }
    return {};
}

}  // namespace

StorageId::StorageId(std::string table)
        : table(std::move(table)) {}

auto StorageId::operator==(const StorageId& other) const -> bool {
    return table == other.table;
}

StorageNode::StorageNode(std::string host, std::uint32_t port, StorageState state)
        : host(std::move(host)), port(port), state(state) {}

Registry::Registry(bool auto_start_storage, std::string storage_binary, std::string storage_root)
        : next_port_(9001),
          auto_start_storage_(auto_start_storage),
          storage_binary_(std::move(storage_binary)),
          storage_root_(std::move(storage_root)) {}

Registry::~Registry() {
    std::unique_lock lock(nodes_mutex_);
    for (const auto& [id, _] : nodes_) {
        StopProcessNonSync(id);
    }
}

auto Registry::New(bool auto_start_storage, std::string storage_binary, std::string storage_root) -> std::shared_ptr<Registry> {
    return std::make_shared<Registry>(auto_start_storage, std::move(storage_binary), std::move(storage_root));
}

auto Registry::CreateNode(const StorageId &id) -> bool {
    std::unique_lock lock(nodes_mutex_);

    if (HasNodeNonSync(id)) {
        return true;
    }

    StorageNode node{"127.0.0.1", next_port_++, StorageState::Unknown};
    if (auto_start_storage_ && !StartProcessNonSync(id, node)) {
        return false;
    }

    nodes_[id] = std::move(node);

    return true;
}

auto Registry::GetNode(const StorageId &id) const -> std::optional<StorageNode> {
    std::shared_lock lock(nodes_mutex_);

    auto iterator = nodes_.find(id);

    if (iterator == nodes_.end()) {
        return std::nullopt;
    }

    return iterator->second;
}

auto Registry::HasNode(const StorageId &id) const -> bool {
    std::shared_lock lock(nodes_mutex_);

    return HasNodeNonSync(id);
}

auto Registry::UpdateNode(const StorageId &id, StorageState state) -> bool {
    std::unique_lock lock(nodes_mutex_);

    if (!HasNodeNonSync(id)) {
        return false;
    }

    nodes_[id].state = state;

    return true;
}

auto Registry::DropNode(const StorageId &id) -> bool {
    std::unique_lock lock(nodes_mutex_);

    if (!HasNodeNonSync(id)) {
        return false;
    }

    StopProcessNonSync(id);
    nodes_.erase(id);

    return true;
}

auto Registry::GetNodes() const -> std::vector<std::pair<StorageId, StorageNode>> {
    std::shared_lock lock(nodes_mutex_);

    std::vector<std::pair<StorageId, StorageNode>> nodes;
    nodes.reserve(nodes_.size());

    for (const auto& [id, node] : nodes_) {
        nodes.emplace_back(id, node);
    }

    return nodes;
}

auto Registry::HasNodeNonSync(const StorageId &id) const -> bool {
    return nodes_.find(id) != nodes_.end();
}

auto Registry::StartProcessNonSync(const StorageId& id, const StorageNode& node) -> bool {
    const auto binary = ResolveStorageBinary();
    if (binary.empty()) {
        return false;
    }

    const auto data_dir = DataDirFor(id);
    std::filesystem::create_directories(data_dir);
    const auto port = std::to_string(node.port);

    const auto pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        const int dev_null = open("/dev/null", O_RDWR);
        if (dev_null >= 0) {
            dup2(dev_null, STDIN_FILENO);
            dup2(dev_null, STDOUT_FILENO);
            dup2(dev_null, STDERR_FILENO);
            close(dev_null);
        }

        execl(binary.c_str(), binary.c_str(),
              "--host", node.host.c_str(),
              "--port", port.c_str(),
              "--data-dir", data_dir.c_str(),
              static_cast<char*>(nullptr));
        _exit(127);
    }

    processes_[id] = static_cast<int>(pid);
    if (WaitUntilReady(node)) {
        return true;
    }

    StopProcessNonSync(id);
    return false;
}

auto Registry::StopProcessNonSync(const StorageId& id) -> void {
    const auto iterator = processes_.find(id);
    if (iterator == processes_.end()) {
        return;
    }

    const auto pid = iterator->second;
    processes_.erase(iterator);

    kill(pid, SIGTERM);
    for (int i = 0; i < 20; ++i) {
        if (waitpid(pid, nullptr, WNOHANG) == pid) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
}

auto Registry::DataDirFor(const StorageId& id) const -> std::filesystem::path {
    return storage_root_ / SanitizedPath(id.table);
}

auto Registry::ResolveStorageBinary() const -> std::filesystem::path {
    if (!storage_binary_.empty()) {
        return std::filesystem::absolute(storage_binary_);
    }

    const auto cwd = std::filesystem::current_path();
    return ExistingExecutable({
        cwd / "qdb-storage",
        cwd / "cmake-build-debug" / "qdb-storage",
        cwd / "build" / "qdb-storage",
        cwd / "apps" / "storage" / "qdb-storage",
    });
}

auto Registry::WaitUntilReady(const StorageNode& node) -> bool {
    for (int i = 0; i < 40; ++i) {
        auto client = qdb::core::TcpClient::New(node.host, node.port);
        if (client && client->Connect()) {
            client->Disconnect();
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

}  // namespace qdb::server

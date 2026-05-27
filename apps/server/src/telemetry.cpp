#include <qdb/server/telemetry.h>

namespace qdb::server {

Telemetry::Telemetry()
    : output_thread_([this] { OutputLoop(); }) {
}

Telemetry::~Telemetry() {
    running_.store(false, std::memory_order_relaxed);
    if (output_thread_.joinable()) {
        output_thread_.join();
    }
}

void Telemetry::RecordRequest(std::uint64_t duration_ms, bool success) {
    total_requests_.fetch_add(1, std::memory_order_relaxed);
    total_duration_ms_.fetch_add(duration_ms, std::memory_order_relaxed);

    if (!success) {
        total_errors_.fetch_add(1, std::memory_order_relaxed);
    }
}

auto Telemetry::GetCurrentRPS() const -> double {
    return rps_window_.Avg();
}

auto Telemetry::GetAvgRPS10min() const -> double {
    return rps_10min_.Avg();
}

auto Telemetry::GetMaxRPS10min() const -> std::uint64_t {
    return rps_10min_.Max();
}

auto Telemetry::GetAvgDuration() const -> double {
    auto total = total_requests_.load(std::memory_order_relaxed);
    if (total == 0) return 0.0;
    auto dur = total_duration_ms_.load(std::memory_order_relaxed);
    return static_cast<double>(dur) / total;
}

auto Telemetry::GetErrorRate() const -> double {
    auto total = total_requests_.load(std::memory_order_relaxed);
    if (total == 0) return 0.0;
    auto err = total_errors_.load(std::memory_order_relaxed);
    return static_cast<double>(err) / total;
}

auto Telemetry::GetTotalRequests() const -> std::uint64_t {
    return total_requests_.load(std::memory_order_relaxed);
}

auto Telemetry::GetTotalErrors() const -> std::uint64_t {
    return total_errors_.load(std::memory_order_relaxed);
}

void Telemetry::OutputLoop() {
    auto next = std::chrono::steady_clock::now() + std::chrono::seconds(1);

    while (running_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_until(next);
        next += std::chrono::seconds(1);

        auto total = total_requests_.load(std::memory_order_relaxed);
        auto rps = total - last_total_requests_;
        last_total_requests_ = total;

        rps_window_.Tick(rps);
        rps_10min_.Tick(rps);
    }
}

}  // namespace qdb::server

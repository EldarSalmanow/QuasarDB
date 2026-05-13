#include <qdb/server/telemetry.h>

#include <iomanip>
#include <sstream>

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

auto Telemetry::GetReport() const -> std::string {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "[TELEMETRY] requests=" << GetTotalRequests()
        << " errors=" << GetTotalErrors()
        << " error_rate=" << (GetErrorRate() * 100) << "%"
        << " rps=" << GetCurrentRPS()
        << " avg_rps_10min=" << GetAvgRPS10min()
        << " max_rps_10min=" << GetMaxRPS10min()
        << " avg_duration=" << GetAvgDuration() << "ms";
    return oss.str();
}

void Telemetry::OutputLoop() {
    auto next = std::chrono::steady_clock::now() + std::chrono::seconds(1);

    while (running_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_until(next);
        next += std::chrono::seconds(1);

        auto req_count = total_requests_.load(std::memory_order_relaxed);
        auto err_count = total_errors_.load(std::memory_order_relaxed);

        rps_window_.Tick(req_count);
        rps_10min_.Tick(req_count);
        duration_window_.Tick(err_count);
    }
}

}  // namespace qdb::server

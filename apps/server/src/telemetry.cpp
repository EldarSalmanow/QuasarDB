#include <qdb/server/telemetry.h>

namespace qdb::server {

Telemetry::Telemetry() : output_thread_([this] { OutputLoop(); }) {}

Telemetry::~Telemetry() {
    running_.store(false, std::memory_order_relaxed);
    if (output_thread_.joinable()) {
        output_thread_.join();
    }
}

void Telemetry::RecordRequest(std::uint64_t duration_ms, bool success) {
    total_requests_.fetch_add(1, std::memory_order_relaxed);
    current_second_requests_.fetch_add(1, std::memory_order_relaxed);
    current_second_duration_ms_.fetch_add(duration_ms, std::memory_order_relaxed);

    if (!success) {
        total_errors_.fetch_add(1, std::memory_order_relaxed);
        current_second_errors_.fetch_add(1, std::memory_order_relaxed);
    }
}

auto Telemetry::GetCurrentRPS() const -> double {
    auto current = current_second_requests_.load(std::memory_order_relaxed);
    return static_cast<double>(current == 0 ? current_rps_.load(std::memory_order_relaxed) : current);
}

auto Telemetry::GetAvgRPS10min() const -> double { return rps_10min_.Avg(); }

auto Telemetry::GetMaxRPS10min() const -> std::uint64_t { return rps_10min_.Max(); }

auto Telemetry::GetAvgDuration() const -> double {
    auto count = duration_count_10s_.Sum() + current_second_requests_.load(std::memory_order_relaxed);
    if (count == 0) return 0.0;
    auto duration = duration_sum_10s_.Sum() + current_second_duration_ms_.load(std::memory_order_relaxed);
    return static_cast<double>(duration) / count;
}

auto Telemetry::GetErrorRate() const -> double {
    auto requests = request_sum_60s_.Sum() + current_second_requests_.load(std::memory_order_relaxed);
    if (requests == 0) return 0.0;
    auto errors = error_sum_60s_.Sum() + current_second_errors_.load(std::memory_order_relaxed);
    return static_cast<double>(errors) / requests;
}

auto Telemetry::GetTotalRequests() const -> std::uint64_t { return total_requests_.load(std::memory_order_relaxed); }

auto Telemetry::GetTotalErrors() const -> std::uint64_t { return total_errors_.load(std::memory_order_relaxed); }

void Telemetry::OutputLoop() {
    auto next = std::chrono::steady_clock::now() + std::chrono::seconds(1);

    while (running_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_until(next);
        next += std::chrono::seconds(1);

        auto total = total_requests_.load(std::memory_order_relaxed);
        auto rps = total - last_total_requests_;
        last_total_requests_ = total;
        current_rps_.store(rps, std::memory_order_relaxed);

        rps_10min_.Tick(rps);
        duration_sum_10s_.Tick(current_second_duration_ms_.exchange(0, std::memory_order_relaxed));
        duration_count_10s_.Tick(current_second_requests_.exchange(0, std::memory_order_relaxed));
        error_sum_60s_.Tick(current_second_errors_.exchange(0, std::memory_order_relaxed));
        request_sum_60s_.Tick(rps);
    }
}

}  // namespace qdb::server

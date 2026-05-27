#ifndef QUASARDB_TELEMETRY_H
#define QUASARDB_TELEMETRY_H

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace qdb::server {

template <size_t N>
class RollingWindow {
public:
    RollingWindow();
    void Tick(std::uint64_t value);
    auto Sum() const -> std::uint64_t;
    auto Avg() const -> double;
    auto Max() const -> std::uint64_t;

private:
    std::array<std::atomic<std::uint64_t>, N> slots_;
    std::atomic<size_t> index_;
};

class Telemetry {
public:
    Telemetry();
    ~Telemetry();

    void RecordRequest(std::uint64_t duration_ms, bool success);

    auto GetCurrentRPS() const -> double;
    auto GetAvgRPS10min() const -> double;
    auto GetMaxRPS10min() const -> std::uint64_t;
    auto GetAvgDuration() const -> double;
    auto GetErrorRate() const -> double;
    auto GetTotalRequests() const -> std::uint64_t;
    auto GetTotalErrors() const -> std::uint64_t;

private:
    void OutputLoop();

    std::atomic<std::uint64_t> total_requests_{0};
    std::atomic<std::uint64_t> total_errors_{0};
    std::atomic<std::uint64_t> total_duration_ms_{0};
    std::uint64_t last_total_requests_{0};

    RollingWindow<10> rps_window_;
    RollingWindow<600> rps_10min_;

    std::thread output_thread_;
    std::atomic<bool> running_{true};
};

template <size_t N>
RollingWindow<N>::RollingWindow() : index_(0) {
    for (auto& slot : slots_) {
        slot.store(0, std::memory_order_relaxed);
    }
}

template <size_t N>
void RollingWindow<N>::Tick(std::uint64_t value) {
    slots_[index_.load(std::memory_order_relaxed)].store(value, std::memory_order_relaxed);
    index_.store((index_.load(std::memory_order_relaxed) + 1) % N, std::memory_order_relaxed);
}

template <size_t N>
auto RollingWindow<N>::Sum() const -> std::uint64_t {
    std::uint64_t sum = 0;
    for (const auto& slot : slots_) {
        sum += slot.load(std::memory_order_relaxed);
    }
    return sum;
}

template <size_t N>
auto RollingWindow<N>::Avg() const -> double {
    return static_cast<double>(Sum()) / N;
}

template <size_t N>
auto RollingWindow<N>::Max() const -> std::uint64_t {
    std::uint64_t max = 0;
    for (const auto& slot : slots_) {
        auto val = slot.load(std::memory_order_relaxed);
        if (val > max) max = val;
    }
    return max;
}

}  // namespace qdb::server

#endif  // QUASARDB_TELEMETRY_H

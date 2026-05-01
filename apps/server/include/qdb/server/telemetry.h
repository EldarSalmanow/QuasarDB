#ifndef QUASARDB_TELEMETRY_H
#define QUASARDB_TELEMETRY_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

namespace qdb::server {

/**
 * @brief Rolling window for time-series metrics
 * 
 * Thread-safe circular buffer for storing metrics over time.
 * Uses atomics to avoid locks.
 */
template <size_t WindowSize>
class RollingWindow {
public:
    RollingWindow() : current_index_(0) {
        for (size_t i = 0; i < WindowSize; ++i) {
            values_[i].store(0, std::memory_order_relaxed);
        }
    }

    /**
     * @brief Add a value to the window
     * @param value Value to add
     */
    void push(uint64_t value) {
        size_t index = current_index_.fetch_add(1, std::memory_order_relaxed) % WindowSize;
        values_[index].store(value, std::memory_order_relaxed);
    }

    /**
     * @brief Get average value in the window
     * @return Average value
     */
    double getAverage() const {
        uint64_t sum = 0;
        for (size_t i = 0; i < WindowSize; ++i) {
            sum += values_[i].load(std::memory_order_relaxed);
        }
        return static_cast<double>(sum) / WindowSize;
    }

    /**
     * @brief Get maximum value in the window
     * @return Maximum value
     */
    uint64_t getMax() const {
        uint64_t max_val = 0;
        for (size_t i = 0; i < WindowSize; ++i) {
            uint64_t val = values_[i].load(std::memory_order_relaxed);
            if (val > max_val) {
                max_val = val;
            }
        }
        return max_val;
    }

    /**
     * @brief Get sum of all values in the window
     * @return Sum of values
     */
    uint64_t getSum() const {
        uint64_t sum = 0;
        for (size_t i = 0; i < WindowSize; ++i) {
            sum += values_[i].load(std::memory_order_relaxed);
        }
        return sum;
    }

private:
    std::atomic<uint64_t> values_[WindowSize];
    std::atomic<size_t> current_index_;
};

/**
 * @brief Telemetry system for collecting server metrics
 * 
 * Collects metrics without blocking using atomics:
 * - RPS (requests per second)
 * - Average/Max RPS over 10 minutes
 * - Average request duration
 * - Error rate
 */
class Telemetry {
public:
    Telemetry();
    ~Telemetry();

    // Disable copy and move
    Telemetry(const Telemetry&) = delete;
    Telemetry& operator=(const Telemetry&) = delete;
    Telemetry(Telemetry&&) = delete;
    Telemetry& operator=(Telemetry&&) = delete;

    /**
     * @brief Record a completed request
     * @param duration_ms Request duration in milliseconds
     * @param success Whether the request succeeded
     */
    void recordRequest(uint64_t duration_ms, bool success);

    /**
     * @brief Get current RPS (last second)
     * @return Requests per second
     */
    uint64_t getCurrentRPS() const;

    /**
     * @brief Get average RPS over 10 minutes
     * @return Average RPS
     */
    double getAvgRPS10min() const;

    /**
     * @brief Get maximum RPS over 10 minutes
     * @return Maximum RPS
     */
    uint64_t getMaxRPS10min() const;

    /**
     * @brief Get average request duration over 10 seconds
     * @return Average duration in milliseconds
     */
    double getAvgDuration() const;

    /**
     * @brief Get error rate over last minute
     * @return Error rate (0.0 to 1.0)
     */
    double getErrorRate() const;

    /**
     * @brief Get total number of requests
     * @return Total requests
     */
    uint64_t getTotalRequests() const;

    /**
     * @brief Get total number of errors
     * @return Total errors
     */
    uint64_t getTotalErrors() const;

    /**
     * @brief Start metrics output thread
     * @param interval_seconds Interval between outputs in seconds
     */
    void startMetricsOutput(int interval_seconds = 10);

    /**
     * @brief Stop metrics output thread
     */
    void stopMetricsOutput();

private:
    void metricsOutputLoop(int interval_seconds);
    void updatePerSecondMetrics();

    // Total counters
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> total_errors_{0};
    std::atomic<uint64_t> total_duration_ms_{0};

    // Per-second counters (reset every second)
    std::atomic<uint64_t> requests_this_second_{0};
    std::atomic<uint64_t> errors_this_second_{0};

    // Rolling windows
    RollingWindow<10> rps_10sec_;           // RPS over 10 seconds
    RollingWindow<600> rps_10min_;          // RPS over 10 minutes (600 seconds)
    RollingWindow<10> avg_duration_10sec_;  // Avg duration over 10 seconds
    RollingWindow<60> errors_per_min_;      // Errors per second over 60 seconds

    // Metrics output thread
    std::unique_ptr<std::thread> metrics_thread_;
    std::atomic<bool> stop_metrics_thread_{false};

    // Update thread for per-second metrics
    std::unique_ptr<std::thread> update_thread_;
    std::atomic<bool> stop_update_thread_{false};
};

}  // namespace qdb::server

#endif  // QUASARDB_TELEMETRY_H

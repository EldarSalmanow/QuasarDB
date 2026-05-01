#include <qdb/server/telemetry.h>

#include <iostream>
#include <iomanip>

namespace qdb::server {

Telemetry::Telemetry() {
    // Start update thread for per-second metrics
    stop_update_thread_.store(false, std::memory_order_relaxed);
    update_thread_ = std::make_unique<std::thread>(&Telemetry::updatePerSecondMetrics, this);
}

Telemetry::~Telemetry() {
    stopMetricsOutput();
    
    // Stop update thread
    stop_update_thread_.store(true, std::memory_order_relaxed);
    if (update_thread_ && update_thread_->joinable()) {
        update_thread_->join();
    }
}

void Telemetry::recordRequest(uint64_t duration_ms, bool success) {
    // Update total counters
    total_requests_.fetch_add(1, std::memory_order_relaxed);
    total_duration_ms_.fetch_add(duration_ms, std::memory_order_relaxed);
    
    // Update per-second counters
    requests_this_second_.fetch_add(1, std::memory_order_relaxed);
    
    if (!success) {
        total_errors_.fetch_add(1, std::memory_order_relaxed);
        errors_this_second_.fetch_add(1, std::memory_order_relaxed);
    }
}

uint64_t Telemetry::getCurrentRPS() const {
    return requests_this_second_.load(std::memory_order_relaxed);
}

double Telemetry::getAvgRPS10min() const {
    return rps_10min_.getAverage();
}

uint64_t Telemetry::getMaxRPS10min() const {
    return rps_10min_.getMax();
}

double Telemetry::getAvgDuration() const {
    uint64_t total_reqs = total_requests_.load(std::memory_order_relaxed);
    if (total_reqs == 0) {
        return 0.0;
    }
    
    uint64_t total_dur = total_duration_ms_.load(std::memory_order_relaxed);
    return static_cast<double>(total_dur) / total_reqs;
}

double Telemetry::getErrorRate() const {
    uint64_t total_errors_in_window = errors_per_min_.getSum();
    uint64_t total_requests_in_window = rps_10min_.getSum();
    
    if (total_requests_in_window == 0) {
        return 0.0;
    }
    
    return static_cast<double>(total_errors_in_window) / total_requests_in_window;
}

uint64_t Telemetry::getTotalRequests() const {
    return total_requests_.load(std::memory_order_relaxed);
}

uint64_t Telemetry::getTotalErrors() const {
    return total_errors_.load(std::memory_order_relaxed);
}

void Telemetry::startMetricsOutput(int interval_seconds) {
    if (metrics_thread_) {
        return; // Already running
    }
    
    stop_metrics_thread_.store(false, std::memory_order_relaxed);
    metrics_thread_ = std::make_unique<std::thread>(
        &Telemetry::metricsOutputLoop, this, interval_seconds
    );
}

void Telemetry::stopMetricsOutput() {
    if (!metrics_thread_) {
        return;
    }
    
    stop_metrics_thread_.store(true, std::memory_order_relaxed);
    if (metrics_thread_->joinable()) {
        metrics_thread_->join();
    }
    metrics_thread_.reset();
}

void Telemetry::metricsOutputLoop(int interval_seconds) {
    while (!stop_metrics_thread_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
        
        if (stop_metrics_thread_.load(std::memory_order_relaxed)) {
            break;
        }
        
        // Print metrics
        std::cout << "\n=== QuasarDB Telemetry ===" << std::endl;
        std::cout << "Total Requests: " << getTotalRequests() << std::endl;
        std::cout << "Total Errors: " << getTotalErrors() << std::endl;
        std::cout << "Current RPS: " << getCurrentRPS() << std::endl;
        std::cout << "Avg RPS (10min): " << std::fixed << std::setprecision(2) 
                  << getAvgRPS10min() << std::endl;
        std::cout << "Max RPS (10min): " << getMaxRPS10min() << std::endl;
        std::cout << "Avg Duration: " << std::fixed << std::setprecision(2) 
                  << getAvgDuration() << " ms" << std::endl;
        std::cout << "Error Rate: " << std::fixed << std::setprecision(4) 
                  << (getErrorRate() * 100.0) << "%" << std::endl;
        std::cout << "==========================\n" << std::endl;
    }
}

void Telemetry::updatePerSecondMetrics() {
    while (!stop_update_thread_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (stop_update_thread_.load(std::memory_order_relaxed)) {
            break;
        }
        
        // Get current second's metrics
        uint64_t rps = requests_this_second_.exchange(0, std::memory_order_relaxed);
        uint64_t errors = errors_this_second_.exchange(0, std::memory_order_relaxed);
        
        // Push to rolling windows
        rps_10sec_.push(rps);
        rps_10min_.push(rps);
        errors_per_min_.push(errors);
    }
}

}  // namespace qdb::server

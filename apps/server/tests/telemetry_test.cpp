#include <qdb/server/telemetry.h>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace qdb::server::test {

// Test basic request recording
TEST(TelemetryTest, RecordRequest) {
    Telemetry telemetry;
    
    telemetry.recordRequest(100, true);
    telemetry.recordRequest(200, true);
    telemetry.recordRequest(150, false);
    
    EXPECT_EQ(telemetry.getTotalRequests(), 3);
    EXPECT_EQ(telemetry.getTotalErrors(), 1);
}

// Test average duration calculation
TEST(TelemetryTest, AverageDuration) {
    Telemetry telemetry;
    
    telemetry.recordRequest(100, true);
    telemetry.recordRequest(200, true);
    telemetry.recordRequest(300, true);
    
    double avg = telemetry.getAvgDuration();
    EXPECT_DOUBLE_EQ(avg, 200.0);
}

// Test RPS tracking
TEST(TelemetryTest, CurrentRPS) {
    Telemetry telemetry;
    
    // Record some requests
    for (int i = 0; i < 10; ++i) {
        telemetry.recordRequest(50, true);
    }
    
    // Current RPS should be 10
    EXPECT_EQ(telemetry.getCurrentRPS(), 10);
}

// Test RollingWindow average
TEST(TelemetryTest, RollingWindowAverage) {
    RollingWindow<5> window;
    
    window.push(10);
    window.push(20);
    window.push(30);
    window.push(40);
    window.push(50);
    
    double avg = window.getAverage();
    EXPECT_DOUBLE_EQ(avg, 30.0);
}

// Test RollingWindow max
TEST(TelemetryTest, RollingWindowMax) {
    RollingWindow<5> window;
    
    window.push(10);
    window.push(50);
    window.push(30);
    window.push(20);
    window.push(40);
    
    uint64_t max_val = window.getMax();
    EXPECT_EQ(max_val, 50);
}

// Test RollingWindow sum
TEST(TelemetryTest, RollingWindowSum) {
    RollingWindow<5> window;
    
    window.push(10);
    window.push(20);
    window.push(30);
    window.push(40);
    window.push(50);
    
    uint64_t sum = window.getSum();
    EXPECT_EQ(sum, 150);
}

// Test RollingWindow wrapping
TEST(TelemetryTest, RollingWindowWrapping) {
    RollingWindow<3> window;
    
    window.push(10);
    window.push(20);
    window.push(30);
    
    // Average should be (10 + 20 + 30) / 3 = 20
    EXPECT_DOUBLE_EQ(window.getAverage(), 20.0);
    
    // Push more values, should wrap around
    window.push(40);
    window.push(50);
    
    // Now window contains: 40, 50, 30 (oldest value 10 and 20 are overwritten)
    // Average should be (40 + 50 + 30) / 3 = 40
    EXPECT_DOUBLE_EQ(window.getAverage(), 40.0);
}

// Test error rate calculation
TEST(TelemetryTest, ErrorRate) {
    Telemetry telemetry;
    
    // Wait for update thread to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Record requests with some errors
    for (int i = 0; i < 100; ++i) {
        telemetry.recordRequest(50, i % 10 != 0); // 10% error rate
    }
    
    // Wait for metrics to update
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    double error_rate = telemetry.getErrorRate();
    
    // Error rate should be around 0.1 (10%)
    EXPECT_GE(error_rate, 0.05);
    EXPECT_LE(error_rate, 0.15);
}

// Test concurrent request recording
TEST(TelemetryTest, ConcurrentRecording) {
    Telemetry telemetry;
    
    const int num_threads = 10;
    const int requests_per_thread = 100;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&telemetry, requests_per_thread]() {
            for (int j = 0; j < requests_per_thread; ++j) {
                telemetry.recordRequest(50, true);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(telemetry.getTotalRequests(), num_threads * requests_per_thread);
}

// Test metrics output start/stop
TEST(TelemetryTest, MetricsOutputStartStop) {
    Telemetry telemetry;
    
    // Start metrics output
    telemetry.startMetricsOutput(1);
    
    // Record some requests
    for (int i = 0; i < 10; ++i) {
        telemetry.recordRequest(50, true);
    }
    
    // Wait a bit for output
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Stop metrics output
    telemetry.stopMetricsOutput();
    
    // Should not crash
    SUCCEED();
}

// Test zero requests scenario
TEST(TelemetryTest, ZeroRequests) {
    Telemetry telemetry;
    
    EXPECT_EQ(telemetry.getTotalRequests(), 0);
    EXPECT_EQ(telemetry.getTotalErrors(), 0);
    EXPECT_DOUBLE_EQ(telemetry.getAvgDuration(), 0.0);
    EXPECT_DOUBLE_EQ(telemetry.getErrorRate(), 0.0);
}

}  // namespace qdb::server::test

#include <qdb/server/telemetry.h>

#include <gtest/gtest.h>

#include <thread>

namespace qdb::server {

TEST(TelemetryTest, RollingWindowSum) {
    RollingWindow<5> w;
    w.Tick(10);
    w.Tick(20);
    w.Tick(30);
    ASSERT_EQ(w.Sum(), 60);
}

TEST(TelemetryTest, RollingWindowMax) {
    RollingWindow<5> w;
    w.Tick(10);
    w.Tick(50);
    w.Tick(30);
    ASSERT_EQ(w.Max(), 50);
}

TEST(TelemetryTest, RollingWindowAvg) {
    RollingWindow<4> w;
    w.Tick(10);
    w.Tick(20);
    w.Tick(30);
    w.Tick(40);
    ASSERT_DOUBLE_EQ(w.Avg(), 25.0);
}

TEST(TelemetryTest, RollingWindowWrapAround) {
    RollingWindow<3> w;
    w.Tick(100);
    w.Tick(200);
    w.Tick(300);
    w.Tick(400);
    ASSERT_EQ(w.Sum(), 900);
}

TEST(TelemetryTest, RecordRequest) {
    Telemetry t;
    t.RecordRequest(50, true);
    t.RecordRequest(30, false);
    ASSERT_EQ(t.GetTotalRequests(), 2);
    ASSERT_EQ(t.GetTotalErrors(), 1);
    ASSERT_DOUBLE_EQ(t.GetAvgDuration(), 40.0);
    ASSERT_DOUBLE_EQ(t.GetErrorRate(), 0.5);
}

TEST(TelemetryTest, NoRequests) {
    Telemetry t;
    ASSERT_EQ(t.GetTotalRequests(), 0);
    ASSERT_DOUBLE_EQ(t.GetAvgDuration(), 0.0);
    ASSERT_DOUBLE_EQ(t.GetErrorRate(), 0.0);
}

TEST(TelemetryTest, AllErrors) {
    Telemetry t;
    t.RecordRequest(10, false);
    t.RecordRequest(20, false);
    ASSERT_EQ(t.GetTotalErrors(), 2);
    ASSERT_DOUBLE_EQ(t.GetErrorRate(), 1.0);
}

TEST(TelemetryTest, ReportFormat) {
    Telemetry t;
    t.RecordRequest(100, true);
    auto report = t.GetReport();
    ASSERT_FALSE(report.empty());
    ASSERT_NE(report.find("[TELEMETRY]"), std::string::npos);
    ASSERT_NE(report.find("requests=1"), std::string::npos);
}

TEST(TelemetryTest, ConcurrentRequests) {
    Telemetry t;
    std::thread t1([&] { for (int i = 0; i < 100; ++i) t.RecordRequest(5, true); });
    std::thread t2([&] { for (int i = 0; i < 100; ++i) t.RecordRequest(10, false); });
    t1.join();
    t2.join();
    ASSERT_EQ(t.GetTotalRequests(), 200);
    ASSERT_EQ(t.GetTotalErrors(), 100);
}

}  // namespace qdb::server

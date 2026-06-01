#include <qdb/server/task_processor.h>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace qdb::server {

static auto MakeHandler(std::atomic<int>& counter, int delay_ms = 0) -> TaskProcessor::Handler {
    return [&counter, delay_ms](const Statement&) -> qdb::core::Response {
        counter.fetch_add(1, std::memory_order_relaxed);
        if (delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
        return qdb::core::ResponseBuilder::Success().Build();
    };
}

TEST(AsyncStatusTest, SubmitReturnsGuid) {
    std::atomic<int> counter{0};
    TaskProcessor tracker(MakeHandler(counter));

    auto guid = tracker.Submit(std::make_unique<SelectStmt>(true, std::vector<SelectItem>{}, TableRef{"users"}));
    ASSERT_FALSE(guid.empty());

    tracker.Stop();
}

TEST(AsyncStatusTest, GetStatusPending) {
    std::atomic<int> counter{0};
    TaskProcessor tracker(MakeHandler(counter, 5000));

    auto guid = tracker.Submit(std::make_unique<SelectStmt>(true, std::vector<SelectItem>{}, TableRef{"users"}));

    auto result = tracker.Get(guid);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->id, guid);
    ASSERT_TRUE(result->status == TaskStatus::Pending || result->status == TaskStatus::Running);

    tracker.Stop();
}

TEST(AsyncStatusTest, GetStatusCompleted) {
    std::atomic<int> counter{0};
    TaskProcessor tracker(MakeHandler(counter, 10));

    auto guid = tracker.Submit(std::make_unique<SelectStmt>(true, std::vector<SelectItem>{}, TableRef{"users"}));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto result = tracker.Get(guid);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->status, TaskStatus::Completed);

    tracker.Stop();
}

TEST(AsyncStatusTest, GetStatusNonexistent) {
    std::atomic<int> counter{0};
    TaskProcessor tracker(MakeHandler(counter));

    auto result = tracker.Get("nonexistent-guid");
    ASSERT_FALSE(result.has_value());

    tracker.Stop();
}

TEST(AsyncStatusTest, CancelTask) {
    std::atomic<int> counter{0};
    TaskProcessor tracker(MakeHandler(counter, 500));

    auto guid = tracker.Submit(std::make_unique<SelectStmt>(true, std::vector<SelectItem>{}, TableRef{"users"}));

    auto cancelled = tracker.Cancel(guid);
    ASSERT_TRUE(cancelled);

    auto result = tracker.Get(guid);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->status, TaskStatus::Failed);
    ASSERT_TRUE(result->error.has_value());
    ASSERT_EQ(result->error.value(), "cancelled");

    tracker.Stop();
}

TEST(AsyncStatusTest, MultipleSubmissions) {
    std::atomic<int> counter{0};
    TaskProcessor tracker(MakeHandler(counter, 10), 4);

    std::vector<std::string> guids;
    for (int i = 0; i < 5; ++i) {
        guids.push_back(tracker.Submit(std::make_unique<SelectStmt>(true, std::vector<SelectItem>{}, TableRef{"users"}))
        );
    }

    ASSERT_EQ(guids.size(), 5);
    for (size_t i = 0; i < guids.size(); ++i) {
        for (size_t j = i + 1; j < guids.size(); ++j) {
            ASSERT_NE(guids[i], guids[j]);
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_GE(counter.load(), 1);

    tracker.Stop();
}

}  // namespace qdb::server

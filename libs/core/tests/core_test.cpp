#include <gtest/gtest.h>

#include <qdb/core/request.h>
#include <qdb/core/response.h>

TEST(CoreBasic, AlwaysPasses) { EXPECT_TRUE(true); }

TEST(RequestEnvelope, QueryUsesActionTokenAndData) {
    auto request = qdb::core::RequestBuilder::Query("SELECT * FROM users;")
                       .Token("jwt")
                       .Build();

    const auto json = request.ToJsonObject();
    EXPECT_EQ(json.at("action"), "query");
    EXPECT_EQ(json.at("token"), "jwt");
    EXPECT_EQ(json.at("data").at("query"), "SELECT * FROM users;");
    EXPECT_EQ(request.Query(), "SELECT * FROM users;");
}

TEST(RequestEnvelope, ParsesCheckTask) {
    const auto request = qdb::core::Request::FromJsonObject({
        {"action", "check_task"},
        {"data", {{"task_id", "task-1"}}},
    });

    EXPECT_EQ(request.Action(), "check_task");
    ASSERT_TRUE(request.TaskId().has_value());
    EXPECT_EQ(request.TaskId().value(), "task-1");
}

TEST(ResponseEnvelope, SerializesJsonData) {
    auto response = qdb::core::ResponseBuilder::Success()
                        .Message("ok")
                        .Data({{"rows", nlohmann::json::array()}, {"rows_affected", 0}})
                        .Build();

    const auto json = response.ToJsonObject();
    EXPECT_EQ(json.at("status"), "success");
    EXPECT_FALSE(json.contains("code"));
    EXPECT_TRUE(json.at("data").at("rows").is_array());
    EXPECT_TRUE(response.IsSuccess());
}

TEST(ResponseEnvelope, SupportsPendingStatus) {
    auto response = qdb::core::ResponseBuilder::Pending()
                        .Message("running")
                        .Data({{"task_id", "task-1"}})
                        .Build();

    EXPECT_TRUE(response.IsPending());
    EXPECT_EQ(response.ToJsonObject().at("status"), "pending");
}

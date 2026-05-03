#include <gtest/gtest.h>
#include "../include/qdb/storage/sql_bool.h"

TEST(SqlBoolLogicTest, AndStandardSql) {
    EXPECT_EQ(SqlBool::TRUE  && SqlBool::TRUE,  SqlBool::TRUE);
    EXPECT_EQ(SqlBool::TRUE  && SqlBool::FALSE, SqlBool::FALSE);
    
    EXPECT_EQ(SqlBool::FALSE && SqlBool::UNKNOWN, SqlBool::FALSE);
    EXPECT_EQ(SqlBool::UNKNOWN && SqlBool::FALSE, SqlBool::FALSE);
    
    EXPECT_EQ(SqlBool::TRUE  && SqlBool::UNKNOWN, SqlBool::UNKNOWN);
    EXPECT_EQ(SqlBool::UNKNOWN && SqlBool::UNKNOWN, SqlBool::UNKNOWN);
}

TEST(SqlBoolLogicTest, OrStandardSql) {
    EXPECT_EQ(SqlBool::FALSE || SqlBool::FALSE, SqlBool::FALSE);
    EXPECT_EQ(SqlBool::TRUE  || SqlBool::FALSE, SqlBool::TRUE);
    
    EXPECT_EQ(SqlBool::TRUE  || SqlBool::UNKNOWN, SqlBool::TRUE);
    EXPECT_EQ(SqlBool::UNKNOWN || SqlBool::TRUE,  SqlBool::TRUE);
    
    EXPECT_EQ(SqlBool::FALSE || SqlBool::UNKNOWN, SqlBool::UNKNOWN);
    EXPECT_EQ(SqlBool::UNKNOWN || SqlBool::UNKNOWN, SqlBool::UNKNOWN);
}

TEST(SqlBoolLogicTest, NotStandardSql) {
    EXPECT_EQ(!SqlBool::TRUE,    SqlBool::FALSE);
    EXPECT_EQ(!SqlBool::FALSE,   SqlBool::TRUE);
    EXPECT_EQ(!SqlBool::UNKNOWN, SqlBool::UNKNOWN);
}

TEST(SqlBoolLogicTest, Equality) {
    EXPECT_TRUE(SqlBool::TRUE == SqlBool::TRUE);
    EXPECT_FALSE(SqlBool::TRUE == SqlBool::FALSE);
    EXPECT_TRUE(SqlBool::UNKNOWN == SqlBool::UNKNOWN);
}

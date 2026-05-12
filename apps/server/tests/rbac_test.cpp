#include <qdb/server/rbac.h>

#include <gtest/gtest.h>

#include <fstream>

namespace qdb::server {

TEST(RBACTest, GrantAndCheckRead) {
    auto test_path = "test_rbac_grant_read.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("alice", "mydb", "users", Permission::READ);
        ASSERT_TRUE(rbac.CheckPermission("alice", "mydb", "users", Permission::READ));
        ASSERT_FALSE(rbac.CheckPermission("alice", "mydb", "users", Permission::WRITE));
        ASSERT_FALSE(rbac.CheckPermission("bob", "mydb", "users", Permission::READ));
    }

    std::remove(test_path);
}

TEST(RBACTest, GrantMultiplePermissions) {
    auto test_path = "test_rbac_multi_perm.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("alice", "mydb", "users", Permission::READ);
        rbac.GrantPermission("alice", "mydb", "users", Permission::WRITE);
        rbac.GrantPermission("alice", "mydb", "users", Permission::CREATE);

        ASSERT_TRUE(rbac.CheckPermission("alice", "mydb", "users", Permission::READ));
        ASSERT_TRUE(rbac.CheckPermission("alice", "mydb", "users", Permission::WRITE));
        ASSERT_TRUE(rbac.CheckPermission("alice", "mydb", "users", Permission::CREATE));
        ASSERT_FALSE(rbac.CheckPermission("alice", "mydb", "users", Permission::DELETE));
    }

    std::remove(test_path);
}

TEST(RBACTest, RevokePermission) {
    auto test_path = "test_rbac_revoke.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("alice", "mydb", "users", Permission::READ);
        rbac.GrantPermission("alice", "mydb", "users", Permission::WRITE);

        ASSERT_TRUE(rbac.CheckPermission("alice", "mydb", "users", Permission::READ));

        rbac.RevokePermission("alice", "mydb", "users", Permission::READ);
        ASSERT_FALSE(rbac.CheckPermission("alice", "mydb", "users", Permission::READ));
        ASSERT_TRUE(rbac.CheckPermission("alice", "mydb", "users", Permission::WRITE));
    }

    std::remove(test_path);
}

TEST(RBACTest, WildcardUser) {
    auto test_path = "test_rbac_wildcard_user.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("*", "mydb", "users", Permission::READ);

        ASSERT_TRUE(rbac.CheckPermission("anyone", "mydb", "users", Permission::READ));
        ASSERT_TRUE(rbac.CheckPermission("alice", "mydb", "users", Permission::READ));
    }

    std::remove(test_path);
}

TEST(RBACTest, WildcardTable) {
    auto test_path = "test_rbac_wildcard_table.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("alice", "mydb", "*", Permission::READ);

        ASSERT_TRUE(rbac.CheckPermission("alice", "mydb", "users", Permission::READ));
        ASSERT_TRUE(rbac.CheckPermission("alice", "mydb", "orders", Permission::READ));
        ASSERT_FALSE(rbac.CheckPermission("alice", "mydb", "orders", Permission::WRITE));
        ASSERT_FALSE(rbac.CheckPermission("bob", "mydb", "users", Permission::READ));
    }

    std::remove(test_path);
}

TEST(RBACTest, WildcardDatabase) {
    auto test_path = "test_rbac_wildcard_db.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("alice", "*", "users", Permission::READ);

        ASSERT_TRUE(rbac.CheckPermission("alice", "db1", "users", Permission::READ));
        ASSERT_TRUE(rbac.CheckPermission("alice", "db2", "users", Permission::READ));
        ASSERT_FALSE(rbac.CheckPermission("alice", "db1", "orders", Permission::READ));
    }

    std::remove(test_path);
}

TEST(RBACTest, GlobalWildcard) {
    auto test_path = "test_rbac_global.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("admin", "*", "*", Permission::READ);
        rbac.GrantPermission("admin", "*", "*", Permission::WRITE);
        rbac.GrantPermission("admin", "*", "*", Permission::CREATE);
        rbac.GrantPermission("admin", "*", "*", Permission::DELETE);

        ASSERT_TRUE(rbac.CheckPermission("admin", "any_db", "any_table", Permission::READ));
        ASSERT_TRUE(rbac.CheckPermission("admin", "any_db", "any_table", Permission::DELETE));
        ASSERT_FALSE(rbac.CheckPermission("user", "any_db", "any_table", Permission::READ));
    }

    std::remove(test_path);
}

TEST(RBACTest, GetUserPermissions) {
    auto test_path = "test_rbac_get_perms.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("alice", "db1", "t1", Permission::READ);
        rbac.GrantPermission("alice", "db1", "t2", Permission::WRITE);

        auto perms = rbac.GetUserPermissions("alice");
        ASSERT_EQ(perms.size(), 2);

        perms = rbac.GetUserPermissions("bob");
        ASSERT_TRUE(perms.empty());
    }

    std::remove(test_path);
}

TEST(RBACTest, Persistence) {
    auto test_path = "test_rbac_persist.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("alice", "mydb", "users", Permission::READ);
    }

    {
        RBACManager rbac(test_path);
        ASSERT_TRUE(rbac.CheckPermission("alice", "mydb", "users", Permission::READ));
    }

    std::remove(test_path);
}

TEST(RBACTest, DuplicatePermission) {
    auto test_path = "test_rbac_duplicate.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("alice", "db", "table", Permission::READ);
        rbac.GrantPermission("alice", "db", "table", Permission::READ);

        ASSERT_TRUE(rbac.CheckPermission("alice", "db", "table", Permission::READ));

        rbac.RevokePermission("alice", "db", "table", Permission::READ);
        ASSERT_FALSE(rbac.CheckPermission("alice", "db", "table", Permission::READ));
    }

    std::remove(test_path);
}

TEST(RBACTest, MultipleUserIsolation) {
    auto test_path = "test_rbac_isolation.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("alice", "db", "t1", Permission::READ);
        rbac.GrantPermission("bob", "db", "t1", Permission::WRITE);

        ASSERT_TRUE(rbac.CheckPermission("alice", "db", "t1", Permission::READ));
        ASSERT_FALSE(rbac.CheckPermission("alice", "db", "t1", Permission::WRITE));
        ASSERT_FALSE(rbac.CheckPermission("bob", "db", "t1", Permission::READ));
        ASSERT_TRUE(rbac.CheckPermission("bob", "db", "t1", Permission::WRITE));
    }

    std::remove(test_path);
}

TEST(RBACTest, RevokeNonexistent) {
    auto test_path = "test_rbac_revoke_nonexist.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        ASSERT_NO_THROW(rbac.RevokePermission("nobody", "db", "table", Permission::READ));
    }

    std::remove(test_path);
}

TEST(RBACTest, CheckNonexistentUser) {
    auto test_path = "test_rbac_check_nonexist.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        ASSERT_FALSE(rbac.CheckPermission("unknown", "db", "table", Permission::READ));
    }

    std::remove(test_path);
}

TEST(RBACTest, EmptyRules) {
    auto test_path = "test_rbac_empty.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        ASSERT_TRUE(rbac.GetAllRules().empty());
    }

    std::remove(test_path);
}

TEST(RBACTest, GetAllRules) {
    auto test_path = "test_rbac_get_all.json";
    std::remove(test_path);

    {
        RBACManager rbac(test_path);
        rbac.GrantPermission("alice", "db", "t1", Permission::READ);
        rbac.GrantPermission("bob", "db", "t2", Permission::WRITE);

        auto rules = rbac.GetAllRules();
        ASSERT_EQ(rules.size(), 2);
    }

    std::remove(test_path);
}

}  // namespace qdb::server

#include <gtest/gtest.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <numeric>
#include <random>
#include <vector>
#include <string_view>
#include <filesystem>
#include "qdb/storage/b_star_plus_tree.h"

namespace fs = std::filesystem;

class BStarPlusTreeTest : public ::testing::Test {
public:
    static constexpr std::string_view test_tree_name = "test_b_star_plus_tree";

protected:
    void SetUp() override {
        if (fs::exists(test_tree_name)) {
            fs::remove(test_tree_name);
        }
    }

    void TearDown() override {
        if (fs::exists(test_tree_name)) {
            fs::remove(test_tree_name);
        }
    }
};

bool randomTest(const std::string& filename, int iterations = 10000) {
    BStarPlusTree<int, int> tree(filename);
    std::vector<int> addedKeys;

    std::random_device rdevice;
    std::mt19937 gen(rdevice());
    std::uniform_int_distribution<> dis(1, 10000);
    std::uniform_int_distribution<> opDis(0, 1);  // 0 - вставка, 1 - удаление

    std::cout << "Starting random test with " << iterations << " iterations..." << std::endl;

    for (int i = 0; i < iterations; ++i) {
        int operation = (addedKeys.empty()) ? 0 : opDis(gen);

        if (operation == 0) {
            int key = dis(gen);
            while (std::find(addedKeys.begin(), addedKeys.end(), key) != addedKeys.end()) {
                key = dis(gen);
            }
            int value = key * 10;
            std::cout << "Step " << i << ": Inserting " << key << std::endl;
            tree.insert(key, value);
            addedKeys.push_back(key);
        } else {
            std::uniform_int_distribution<> indexDis(0, static_cast<int>(addedKeys.size()) - 1);
            int idx = indexDis(gen);
            int key = addedKeys[idx];
            std::cout << "Step " << i << ": Removing " << key << std::endl;
            tree.remove(key);
            addedKeys.erase(addedKeys.begin() + idx);
        }
        // tree.print();
        if (!tree.check_integrity()) {
            std::cout << "Integrity check failed at iteration " << i << std::endl;
            tree.print();
            return false;
        }
    }
    return true;
}

bool randomBigTest(const std::string& filename, int iterations = 10000) {
    BStarPlusTree<int, int> tree(filename);
    std::vector<int> addedKeys;

    std::random_device rdevice;
    std::mt19937 gen(rdevice());
    std::uniform_int_distribution<> dis(1, 100000);

    std::cout << "Starting random test with " << iterations << " iterations..." << std::endl;

    for (int operation : {0, 1}) {
        for (int i = 0; i < iterations; ++i) {
            std::stringstream buffer;
            std::streambuf* oldCoutBuffer = std::cout.rdbuf(buffer.rdbuf());
            tree.print();
            std::cout.rdbuf(oldCoutBuffer);

            if (operation == 0) {
                int key = dis(gen);
                while (std::find(addedKeys.begin(), addedKeys.end(), key) != addedKeys.end()) {
                    key = dis(gen);
                }
                int value = key * 10;
                std::cout << "Step " << i << ": Inserting " << key << std::endl;
                tree.insert(key, value);
                addedKeys.push_back(key);
            } else {
                std::uniform_int_distribution<> indexDis(0, static_cast<int>(addedKeys.size()) - 1);
                int idx = indexDis(gen);
                int key = addedKeys[idx];
                std::cout << "Step " << i << ": Removing " << key << std::endl;
                tree.remove(key);
                addedKeys.erase(addedKeys.begin() + idx);
            }
            
            if (!tree.check_integrity()) {
                std::cout << "Integrity check failed at iteration " << i << std::endl;
                std::cout << buffer.str();
                std::cout << "-------------------------" << std::endl;
                tree.print();
                return false;
            }
        }
    }
    return true;
}

TEST_F(BStarPlusTreeTest, BStarPlusTreeRandomTest1) {
    EXPECT_TRUE(randomTest(std::string(BStarPlusTreeTest::test_tree_name)));
}

TEST_F(BStarPlusTreeTest, BStarPlusTreeRandomBigTest1) {
    EXPECT_TRUE(randomBigTest(std::string(BStarPlusTreeTest::test_tree_name)));
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
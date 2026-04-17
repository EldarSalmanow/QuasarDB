#include <gtest/gtest.h>
#include <qdb/storage/b_star_plus_tree/b_star_plus_tree.h>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

bool randomTest(int iterations = 1000) {
    BStarPlusTree<int, int> tree;
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
            tree.insert(key, value);
            addedKeys.push_back(key);
            std::cout << "Step " << i << ": Inserted " << key << std::endl;
        } else {
            std::uniform_int_distribution<> indexDis(0, static_cast<int>(addedKeys.size()) - 1);
            int idx = indexDis(gen);
            int key = addedKeys[idx];
            tree.remove(key);
            addedKeys.erase(addedKeys.begin() + idx);
            std::cout << "Step " << i << ": Removed " << key << std::endl;
        }
        tree.print();
        if (!tree.check_integrity()) {
            std::cout << "Integrity check failed at iteration " << i << std::endl;
            return false;
        }
    }
    return true;
}

bool randomBigTest(int iterations = 1000) {
    BStarPlusTree<int, int> tree;
    std::vector<int> addedKeys;

    std::random_device rdevice;
    std::mt19937 gen(rdevice());
    std::uniform_int_distribution<> dis(1, 10000);

    std::cout << "Starting random test with " << iterations << " iterations..." << std::endl;

    for (int operation : {0, 1}) {
        for (int i = 0; i < iterations; ++i) {
            if (operation == 0) {
                int key = dis(gen);
                while (std::find(addedKeys.begin(), addedKeys.end(), key) != addedKeys.end()) {
                    key = dis(gen);
                }
                int value = key * 10;
                tree.insert(key, value);
                addedKeys.push_back(key);
                std::cout << "Step " << i << ": Inserted " << key << std::endl;
            } else {
                std::uniform_int_distribution<> indexDis(0, static_cast<int>(addedKeys.size()) - 1);
                int idx = indexDis(gen);
                int key = addedKeys[idx];
                tree.remove(key);
                addedKeys.erase(addedKeys.begin() + idx);
                std::cout << "Step " << i << ": Removed " << key << std::endl;
            }
            tree.print();
            if (!tree.check_integrity()) {
                std::cout << "Integrity check failed at iteration " << i << std::endl;
                return false;
            }
        }
    }
    return true;
}

TEST(StorageTest, BStarPlusTreeRandomTest) {
    EXPECT_TRUE(randomTest());
    EXPECT_TRUE(randomTest());
    EXPECT_TRUE(randomTest());
    EXPECT_TRUE(randomTest());
    EXPECT_TRUE(randomTest());
}

TEST(StorageTest, BStarPlusTreeRandomBigTest) {
    EXPECT_TRUE(randomBigTest());
    EXPECT_TRUE(randomBigTest());
    EXPECT_TRUE(randomBigTest());
    EXPECT_TRUE(randomBigTest());
    EXPECT_TRUE(randomBigTest());
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
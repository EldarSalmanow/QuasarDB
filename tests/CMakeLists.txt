# cmake_minimum_required(VERSION 3.10)
# project(test_app)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror=maybe-uninitialized")

add_executable(b_star_plus_tree_test ./storage/b_star_plus_tree_test.cpp)
target_link_libraries(b_star_plus_tree_test gtest_main)
add_test(NAME b_star_plus_tree_test COMMAND tests)
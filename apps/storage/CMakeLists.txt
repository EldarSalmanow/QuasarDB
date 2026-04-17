set(EXECUTABLE_NAME qdb-storage)
set(STORAGE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(STORAGE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(STORAGE_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)

message(STATUS "QuasarDB-Storage")
message(STATUS "    ExecutableName: ${EXECUTABLE_NAME}")
message(STATUS "    IncludeDir: ${STORAGE_INCLUDE_DIR}")
message(STATUS "    SourceDir: ${STORAGE_SRC_DIR}")
message(STATUS "    TestsDir: ${STORAGE_TESTS_DIR}")

file(GLOB_RECURSE STORAGE_SOURCES CONFIGURE_DEPENDS "${STORAGE_SRC_DIR}/*.cpp")
file(GLOB_RECURSE STORAGE_TESTS CONFIGURE_DEPENDS "${STORAGE_TESTS_DIR}/*.cpp")

add_executable(${EXECUTABLE_NAME} main.cpp ${STORAGE_SOURCES})

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${STORAGE_INCLUDE_DIR})

target_link_libraries(${EXECUTABLE_NAME} PRIVATE qdb::core)

if(BUILD_TESTING AND STORAGE_TESTS)
	add_executable(qdb-storage-tests ${STORAGE_TESTS})

	target_include_directories(qdb-storage-tests PRIVATE ${STORAGE_INCLUDE_DIR})

	target_link_libraries(qdb-storage-tests PRIVATE qdb::core GTest::gtest_main)

	gtest_discover_tests(qdb-storage-tests)
endif()

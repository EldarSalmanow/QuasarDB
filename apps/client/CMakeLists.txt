set(EXECUTABLE_NAME qdb-client)
set(CLIENT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CLIENT_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(CLIENT_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)

message(STATUS "QuasarDB-Client")
message(STATUS "    ExecutableName: ${EXECUTABLE_NAME}")
message(STATUS "    IncludeDir: ${CLIENT_INCLUDE_DIR}")
message(STATUS "    SourceDir: ${CLIENT_SRC_DIR}")
message(STATUS "    TestsDir: ${CLIENT_TESTS_DIR}")

file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "${CLIENT_SRC_DIR}/*.cpp")
file(GLOB_RECURSE CLIENT_TESTS CONFIGURE_DEPENDS "${CLIENT_TESTS_DIR}/*.cpp")

add_executable(${EXECUTABLE_NAME} main.cpp ${CLIENT_SOURCES})

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${CLIENT_INCLUDE_DIR})

target_link_libraries(${EXECUTABLE_NAME} PRIVATE qdb::core)

if(BUILD_TESTING AND CLIENT_TESTS)
	add_executable(qdb-client-tests ${CLIENT_TESTS})

	target_include_directories(qdb-client-tests PRIVATE ${CLIENT_INCLUDE_DIR})

	target_link_libraries(qdb-client-tests PRIVATE qdb::core GTest::gtest_main)

	gtest_discover_tests(qdb-client-tests)
endif()

set(EXECUTABLE_NAME qdb-server)
set(SERVER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SERVER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SERVER_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)

message(STATUS "QuasarDB-Server")
message(STATUS "    ExecutableName: ${EXECUTABLE_NAME}")
message(STATUS "    IncludeDir: ${SERVER_INCLUDE_DIR}")
message(STATUS "    SourceDir: ${SERVER_SRC_DIR}")
message(STATUS "    TestsDir: ${SERVER_TESTS_DIR}")

file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS "${SERVER_SRC_DIR}/*.cpp")
file(GLOB_RECURSE SERVER_TESTS CONFIGURE_DEPENDS "${SERVER_TESTS_DIR}/*.cpp")

add_executable(${EXECUTABLE_NAME} main.cpp ${SERVER_SOURCES})

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${SERVER_INCLUDE_DIR})

target_link_libraries(${EXECUTABLE_NAME} PRIVATE qdb::core)

if(BUILD_TESTING AND SERVER_TESTS)
	add_executable(qdb-server-tests ${SERVER_TESTS})

	target_include_directories(qdb-server-tests PRIVATE ${SERVER_INCLUDE_DIR})

	target_link_libraries(qdb-server-tests PRIVATE qdb::core GTest::gtest_main)

	gtest_discover_tests(qdb-server-tests)
endif()

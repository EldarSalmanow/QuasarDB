set(LIBRARY_NAME qdb-core)
set(LIBRARY_ALIAS qdb::core)
set(CORE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CORE_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)

message(STATUS "QuasarDB-Core")
message(STATUS "    LibraryName: ${LIBRARY_NAME}")
message(STATUS "    LibraryAlias: ${LIBRARY_ALIAS}")
message(STATUS "    IncludeDir: ${CORE_INCLUDE_DIR}")
message(STATUS "    TestsDir: ${CORE_TESTS_DIR}")

file(GLOB_RECURSE CORE_TESTS CONFIGURE_DEPENDS "${CORE_TESTS_DIR}/*.cpp")

add_library(${LIBRARY_NAME} INTERFACE)
add_library(${LIBRARY_ALIAS} ALIAS ${LIBRARY_NAME})

target_include_directories(${LIBRARY_NAME} INTERFACE $<BUILD_INTERFACE:${CORE_INCLUDE_DIR}>)
target_link_libraries(${LIBRARY_NAME} INTERFACE qdb::thirdparty)

if(BUILD_TESTING AND CORE_TESTS)
	add_executable(qdb-core-tests ${CORE_TESTS})

	target_link_libraries(qdb-core-tests PRIVATE ${LIBRARY_ALIAS} GTest::gtest_main)

	gtest_discover_tests(qdb-core-tests)
endif()

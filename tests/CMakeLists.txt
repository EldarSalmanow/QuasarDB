file(GLOB_RECURSE PROJECT_INTEGRATION_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/apps/server/src/*.cpp")
file(GLOB_RECURSE STORAGE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/apps/storage/src/*.cpp")

find_package(OpenSSL REQUIRED)
find_package(jwt-cpp REQUIRED)

add_executable(qdb-integration-tests
	${PROJECT_INTEGRATION_TESTS}
	${SERVER_SOURCES}
	${STORAGE_SOURCES}
)

target_include_directories(qdb-integration-tests
	PRIVATE
		${CMAKE_SOURCE_DIR}/libs/core/include
		${CMAKE_SOURCE_DIR}/apps/server/include
		${CMAKE_SOURCE_DIR}/apps/storage/include
		${CMAKE_SOURCE_DIR}/apps/client/include
)

target_link_libraries(qdb-integration-tests
	PRIVATE
		qdb::core
		GTest::gtest_main
		OpenSSL::SSL
		OpenSSL::Crypto
		jwt-cpp::jwt-cpp
)

gtest_discover_tests(qdb-integration-tests)


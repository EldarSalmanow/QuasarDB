cmake_minimum_required(VERSION 3.14)
project(QuasarDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
	$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall>
	$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wextra>
	$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wpedantic>
)

include(CTest)

find_package(nlohmann_json REQUIRED CONFIG)
find_package(args REQUIRED CONFIG)

if(BUILD_TESTING)
	find_package(GTest REQUIRED CONFIG)
	include(GoogleTest)
endif()

add_library(qdb-thirdparty INTERFACE)
add_library(qdb::thirdparty ALIAS qdb-thirdparty)

target_link_libraries(qdb-thirdparty
	INTERFACE
		nlohmann_json::nlohmann_json
		taywee::args
)

set(PROJECT_DIR   ${CMAKE_SOURCE_DIR})
set(CORE_DIR      ${PROJECT_DIR}/libs/core)
set(CLIENT_DIR    ${PROJECT_DIR}/apps/client)
set(SERVER_DIR    ${PROJECT_DIR}/apps/server)
set(STORAGE_DIR   ${PROJECT_DIR}/apps/storage)
set(BINARY_DIR    ${CMAKE_BINARY_DIR})

message(STATUS "QuasarDB")
message(STATUS "    ProjectDir: ${PROJECT_DIR}")
message(STATUS "    CoreDir: ${CORE_DIR}")
message(STATUS "    ClientDir: ${CLIENT_DIR}")
message(STATUS "    ServerDir: ${SERVER_DIR}")
message(STATUS "    StorageDir: ${STORAGE_DIR}")
message(STATUS "    BinaryDir: ${BINARY_DIR}")

add_subdirectory(libs/core)

add_subdirectory(apps/client)
add_subdirectory(apps/server)
add_subdirectory(apps/storage)

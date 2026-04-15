cmake_minimum_required(VERSION 3.14)
project(QuasarDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(CLANG_TIDY_EXE NAMES clang-tidy)

if(CLANG_TIDY_EXE)
	set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE})

	message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
else()
	message(STATUS "clang-tidy not found; continuing without it")
endif()

add_compile_options(
		$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall>
		$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wextra>
		$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wpedantic>
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

# TODO: add dependencies searching
#message(STATUS "Check dependencies...")

add_subdirectory(libs/core)

# add_subdirectory(apps/client)
# add_subdirectory(apps/server)
# add_subdirectory(apps/storage)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.0
)
FetchContent_MakeAvailable(googletest)

if(CLANG_TIDY_EXE)
    set_target_properties(gtest PROPERTIES CXX_CLANG_TIDY "")
    set_target_properties(gtest_main PROPERTIES CXX_CLANG_TIDY "")
    set_target_properties(gmock PROPERTIES CXX_CLANG_TIDY "")
    set_target_properties(gmock_main PROPERTIES CXX_CLANG_TIDY "")
endif()

enable_testing()

add_subdirectory(tests)
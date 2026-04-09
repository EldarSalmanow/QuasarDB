set(WORKING_DIR      ${CLIENT_DIR})
set(EXECUTABLE_NAME  qdb-client)
set(INCLUDE_DIR      ${CLIENT_DIR}/include)
set(SOURCE_DIR       ${CLIENT_DIR}/src)
set(TESTS_DIR        ${CLIENT_DIR}/tests)

message(STATUS "QuasarDB-Client")
message(STATUS "    WorkingDir: ${CLIENT_DIR}")
message(STATUS "    ExecutableName: ${EXECUTABLE_NAME}")
message(STATUS "    IncludeDir: ${INCLUDE_DIR}")
message(STATUS "    SourceDir: ${SOURCE_DIR}")
message(STATUS "    TestsDir: ${TESTS_DIR}")

add_executable(${EXECUTABLE_NAME}
        main.cpp
)

target_include_directories(${EXECUTABLE_NAME} PRIVATE ${INCLUDE_DIR})

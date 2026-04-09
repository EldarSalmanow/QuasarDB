set(CORE_INCLUDE_DIR ${CORE_DIR}/include)

add_library(qdb-core INTERFACE)

target_include_directories(qdb-core INTERFACE ${CORE_INCLUDE_DIR})

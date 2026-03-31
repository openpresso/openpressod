cmake_minimum_required(VERSION 3.30)
project(Openpressod)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(clang-format)
include(clang-tidy)
include(CTest)

find_package(spdlog REQUIRED)
find_package(toml11 REQUIRED)
find_package(magic_enum REQUIRED)
find_package(openpresso_proto REQUIRED)
find_package(libopenpresso REQUIRED)

add_executable(openpressod)

add_subdirectory(src)

target_link_libraries(openpressod PRIVATE 
    spdlog::spdlog
    openpresso_proto::openpresso_proto  
    libopenpresso::libopenpresso)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

set(TIDY_TARGETS
    config_objs 
    state_manager_objs
)

set(FORMAT_TARGETS
    ${TIDY_TARGETS}
    openpressod
    libopenpresso_mock
    openpressod_config_test
    libopenpresso_config_maker_test
    state_manager_test
)

add_clang_format_targets(clang_format_validate TARGETS ${FORMAT_TARGETS} DRY_RUN)
add_clang_format_targets(clang_format_apply TARGETS ${FORMAT_TARGETS})

add_clang_tidy_targets(clang_tidy_validate TARGETS ${TIDY_TARGETS} ARGS --quiet -p=${CMAKE_BINARY_DIR} --warnings-as-errors=*)
add_clang_tidy_targets(clang_tidy_analyze TARGETS ${TIDY_TARGETS} ARGS --quiet -p=${CMAKE_BINARY_DIR})


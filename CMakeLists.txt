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
find_package(farmhash REQUIRED)

add_library(openpressod_objects OBJECT)

set_property(GLOBAL PROPERTY TIDY_TARGETS)
set_property(GLOBAL PROPERTY FORMAT_TARGETS)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

get_property(TIDY_TARGETS_LIST GLOBAL PROPERTY TIDY_TARGETS)
get_property(FORMAT_TARGETS_LIST GLOBAL PROPERTY FORMAT_TARGETS)

list(APPEND FORMAT_TARGETS_LIST ${TIDY_TARGETS_LIST})

add_clang_format_targets(clang_format_validate TARGETS ${FORMAT_TARGETS_LIST} DRY_RUN)
add_clang_format_targets(clang_format_apply TARGETS ${FORMAT_TARGETS_LIST})

add_clang_tidy_targets(clang_tidy_validate TARGETS ${TIDY_TARGETS_LIST} ARGS --quiet -p=${CMAKE_BINARY_DIR} --warnings-as-errors=*)
add_clang_tidy_targets(clang_tidy_analyze TARGETS ${TIDY_TARGETS_LIST} ARGS --quiet -p=${CMAKE_BINARY_DIR})


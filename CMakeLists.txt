cmake_minimum_required(VERSION 3.30)

set(OPENPRESSOD_VERSION "0.0.0" CACHE STRING "")
set(OPENPRESSOD_VERSION_MAJOR "0" CACHE STRING "")
set(OPENPRESSOD_VERSION_MINOR "0" CACHE STRING "")
set(OPENPRESSOD_VERSION_PATCH "0" CACHE STRING "")
set(OPENPRESSOD_REPO_CHANNEL "testing" CACHE STRING "")

option(DOCS_ONLY "Create targets only to make documentation" OFF)

project(Openpressod VERSION ${OPENPRESSOD_VERSION_MAJOR}.${OPENPRESSOD_VERSION_MINOR}.${OPENPRESSOD_VERSION_PATCH})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(clang-format)
include(clang-tidy)
include(openpressod)
include(openpressod-constants)
include(openpressod-docs)
include(CTest)
include(GNUInstallDirs)
include(FetchContent)

add_subdirectory(docs)

if(NOT DOCS_ONLY)
    declare_clang_tools_targets_lists()

    add_subdirectory(src)

    if(BUILD_TESTING)
        add_subdirectory(test)
    endif()

    process_clang_tools_targets()

    add_subdirectory(packaging)
endif()


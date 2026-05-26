cmake_minimum_required(VERSION 3.30)

set(OPENPRESSOD_VERSION 0.0.0 CACHE STRING "")
set(OPENPRESSOD_VERSION_MAJOR 0 CACHE STRING "")
set(OPENPRESSOD_VERSION_MINOR 0 CACHE STRING "")
set(OPENPRESSOD_VERSION_PATCH 0 CACHE STRING "")

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
include(CTest)
include(GNUInstallDirs)

find_package(spdlog REQUIRED)
find_package(toml11 REQUIRED)
find_package(magic_enum REQUIRED)
find_package(openpresso_proto REQUIRED)
find_package(libopenpresso REQUIRED)

declare_clang_tools_targets_lists()

add_library(openpressod_objects OBJECT)
target_link_libraries(openpressod_objects PUBLIC
    spdlog::spdlog
    openpresso_proto::openpresso_proto  
    libopenpresso::libopenpresso
    toml11::toml11
    magic_enum::magic_enum
)

add_executable(openpressod)
target_link_libraries(openpressod PRIVATE openpressod_objects)

target_compile_definitions(openpressod_objects PUBLIC
    OPENPRESSOD_SOCKET_PATH="${OPENPRESSOD_SOCKET_PATH}"
    OPENPRESSOD_CONFIG_PATH="${OPENPRESSOD_CONFIG_PATH}"
    OPENPRESSOD_DATA_DIR="${OPENPRESSOD_DATA_DIR}"
)

openpressod_targets_compile_warnings(openpressod openpressod_objects)
append_targets_to_clang_tidy_list(openpressod openpressod_objects)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

process_clang_tools_targets()

install(TARGETS openpressod RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

create_deb_package()



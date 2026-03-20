cmake_minimum_required(VERSION 3.30)
project(Openpressod)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(spdlog REQUIRED)
find_package(toml11 REQUIRED)
find_package(magic_enum REQUIRED)
find_package(libopenpresso REQUIRED)

add_executable(openpressod)

add_subdirectory(src)
target_link_libraries(openpressod PRIVATE 
    spdlog::spdlog
    toml11::toml11
    magic_enum::magic_enum
    libopenpresso::libopenpresso)

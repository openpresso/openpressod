find_package(spdlog REQUIRED)
find_package(toml11 REQUIRED)
find_package(magic_enum REQUIRED)
find_package(openpresso_proto REQUIRED)
find_package(libopenpresso REQUIRED)

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

target_sources(openpressod 
    PRIVATE main.cpp
)

target_include_directories(openpressod_objects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(config)
add_subdirectory(buttons)
add_subdirectory(daemon)
add_subdirectory(state_manager)
add_subdirectory(async_events_dispatcher)
add_subdirectory(signals_handler)
add_subdirectory(leds)
add_subdirectory(service)

install(TARGETS openpressod RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

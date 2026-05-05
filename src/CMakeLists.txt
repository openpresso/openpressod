add_executable(openpressod
    main.cpp
)

target_include_directories(openpressod_objects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(openpressod_objects PUBLIC
    spdlog::spdlog
    openpresso_proto::openpresso_proto  
    libopenpresso::libopenpresso
    toml11::toml11
    magic_enum::magic_enum
    xxHash::xxhash
)

add_subdirectory(config)
add_subdirectory(buttons)
add_subdirectory(state_manager)
add_subdirectory(async_events_dispatcher)
add_subdirectory(signals_handler)
add_subdirectory(leds)
add_subdirectory(service)

target_link_libraries(openpressod PRIVATE
    openpressod_objects
)

set_property(GLOBAL APPEND PROPERTY TIDY_TARGETS openpressod openpressod_objects)
 

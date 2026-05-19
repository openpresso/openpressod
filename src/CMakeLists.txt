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

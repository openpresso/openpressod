add_library(libopenpresso_mock INTERFACE)
target_sources(libopenpresso_mock PUBLIC FILE_SET HEADERS
    FILES 
        libopenpresso_core_mock.hpp
        brew_profiler_mock.hpp
        controller_base_mock.hpp
        flow_rate_controller_mock.hpp
        logical_input_mock.hpp
        logical_output_mock.hpp
        pressure_controller_mock.hpp
        pressure_sensor_mock.hpp
        temperature_controller_mock.hpp
        temperature_sensor_mock.hpp
        weight_sensor_mock.hpp
)

target_link_libraries(libopenpresso_mock INTERFACE GTest::gmock libopenpresso::libopenpresso)

add_library(openpressod_mock INTERFACE)
target_sources(openpressod_mock PUBLIC FILE_SET HEADERS
    FILES 
        buttons_event_sink_mock.hpp
        leds_handler_mock.hpp
)

target_link_libraries(openpressod_mock INTERFACE GTest::gmock openpressod_objects)

append_targets_to_clang_format_list(libopenpresso_mock openpressod_mock)
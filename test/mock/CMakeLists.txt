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
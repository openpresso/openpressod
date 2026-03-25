#ifndef LIBOPENPRESSO_MOCK_PRESSURE_SENSOR_HPP
#define LIBOPENPRESSO_MOCK_PRESSURE_SENSOR_HPP

#include <gmock/gmock.h>
#include <libopenpresso/interfaces/pressure_sensor.hpp>

class MockPressureSensor : public libopenpresso::interfaces::PressureSensor {
public:
  MOCK_METHOD(libopenpresso::millibars_t, getPressure, (), (const, override));
  MOCK_METHOD(std::optional<libopenpresso::time_delta_t>, fixedUpdateRate, (), (const, noexcept, override));
  MOCK_METHOD(libopenpresso::callback_descriptor_t, registerCallback, (const callback_t&), (override));
  MOCK_METHOD(void, unregisterCallback, (libopenpresso::callback_descriptor_t), (override));
};

#endif // LIBOPENPRESSO_MOCK_PRESSURE_SENSOR_HPP

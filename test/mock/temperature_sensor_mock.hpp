#ifndef LIBOPENPRESSO_MOCK_TEMPERATURE_SENSOR_HPP
#define LIBOPENPRESSO_MOCK_TEMPERATURE_SENSOR_HPP

#include <gmock/gmock.h>
#include <libopenpresso/interfaces/temperature_sensor.hpp>

class MockTemperatureSensor : public libopenpresso::interfaces::TemperatureSensor {
public:
  MOCK_METHOD(libopenpresso::millidegrees_t, getTemperature, (), (const, override));
  MOCK_METHOD(std::optional<libopenpresso::time_delta_t>, fixedUpdateRate, (), (const, noexcept, override));
  MOCK_METHOD(libopenpresso::callback_descriptor_t, registerCallback, (const callback_t&), (override));
  MOCK_METHOD(void, unregisterCallback, (libopenpresso::callback_descriptor_t), (override));
};

#endif // LIBOPENPRESSO_MOCK_TEMPERATURE_SENSOR_HPP

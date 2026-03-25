#ifndef LIBOPENPRESSO_MOCK_WEIGHT_SENSOR_HPP
#define LIBOPENPRESSO_MOCK_WEIGHT_SENSOR_HPP

#include <gmock/gmock.h>
#include <libopenpresso/interfaces/weight_sensor.hpp>

class MockWeightSensor : public libopenpresso::interfaces::WeightSensor {
public:
  MOCK_METHOD(void, tare, (), (override));
  MOCK_METHOD(libopenpresso::milligrams_t, getWeight, (), (const, override));
  MOCK_METHOD(libopenpresso::milligrams_p_second_t, getFlowRate, (), (const, override));
  MOCK_METHOD(std::optional<libopenpresso::time_delta_t>, fixedUpdateRate, (), (const, noexcept, override));
  MOCK_METHOD(libopenpresso::callback_descriptor_t, registerCallback, (const callback_t&), (override));
  MOCK_METHOD(void, unregisterCallback, (libopenpresso::callback_descriptor_t), (override));
};

#endif // LIBOPENPRESSO_MOCK_WEIGHT_SENSOR_HPP

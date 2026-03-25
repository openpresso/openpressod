#ifndef LIBOPENPRESSO_MOCK_TEMPERATURE_CONTROLLER_HPP
#define LIBOPENPRESSO_MOCK_TEMPERATURE_CONTROLLER_HPP

#include <gmock/gmock.h>
#include <libopenpresso/interfaces/temperature_controller.hpp>

class MockTemperatureController : public libopenpresso::interfaces::TemperatureController {
public:
  // TemperatureController methods
  MOCK_METHOD(libopenpresso::millidegrees_t, getTargetTemperature, (), (const, override));
  MOCK_METHOD(void, setTargetTemperature, (libopenpresso::millidegrees_t millidegrees), (override));

  // ControllerBase methods
  MOCK_METHOD(void, activate, (), (override));
  MOCK_METHOD(void, deactivate, (), (override));
  MOCK_METHOD(bool, isActive, (), (const, noexcept, override));
};

#endif // LIBOPENPRESSO_MOCK_TEMPERATURE_CONTROLLER_HPP

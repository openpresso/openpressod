#ifndef LIBOPENPRESSO_MOCK_PRESSURE_CONTROLLER_HPP
#define LIBOPENPRESSO_MOCK_PRESSURE_CONTROLLER_HPP

#include <gmock/gmock.h>
#include <libopenpresso/interfaces/pressure_controller.hpp>

class MockPressureController : public libopenpresso::interfaces::PressureController {
public:
  // PressureController methods
  MOCK_METHOD(libopenpresso::millibars_t, getTargetPressure, (), (const, override));
  MOCK_METHOD(void, setTargetPressure, (libopenpresso::millibars_t millibars), (override));

  // ControllerBase methods
  MOCK_METHOD(void, activate, (), (override));
  MOCK_METHOD(void, deactivate, (), (override));
  MOCK_METHOD(bool, isActive, (), (const, noexcept, override));
};

#endif // LIBOPENPRESSO_MOCK_PRESSURE_CONTROLLER_HPP

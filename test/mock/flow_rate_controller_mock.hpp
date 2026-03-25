#ifndef LIBOPENPRESSO_MOCK_FLOW_RATE_CONTROLLER_HPP
#define LIBOPENPRESSO_MOCK_FLOW_RATE_CONTROLLER_HPP

#include <gmock/gmock.h>
#include <libopenpresso/interfaces/flow_rate_controller.hpp>

class MockFlowRateController : public libopenpresso::interfaces::FlowRateController {
public:
  // FlowRateController methods
  MOCK_METHOD(libopenpresso::milligrams_p_second_t, getTargetRate, (), (const, override));
  MOCK_METHOD(void, setTargetRate, (libopenpresso::milligrams_p_second_t rate), (override));

  // ControllerBase methods
  MOCK_METHOD(void, activate, (), (override));
  MOCK_METHOD(void, deactivate, (), (override));
  MOCK_METHOD(bool, isActive, (), (const, noexcept, override));
};

#endif // LIBOPENPRESSO_MOCK_FLOW_RATE_CONTROLLER_HPP

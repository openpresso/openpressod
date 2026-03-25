#ifndef LIBOPENPRESSO_MOCK_LOGICAL_OUTPUT_HPP
#define LIBOPENPRESSO_MOCK_LOGICAL_OUTPUT_HPP

#include <gmock/gmock.h>
#include <libopenpresso/interfaces/logical_output.hpp>

class MockLogicalOutput : public libopenpresso::interfaces::LogicalOutput {
public:
  // LogicalOutput methods
  MOCK_METHOD(bool, getState, (), (const, override));
  MOCK_METHOD(void, setState, (bool state), (override));

  // ControllerBase methods
  MOCK_METHOD(void, activate, (), (override));
  MOCK_METHOD(void, deactivate, (), (override));
  MOCK_METHOD(bool, isActive, (), (const, noexcept, override));
};

#endif // LIBOPENPRESSO_MOCK_LOGICAL_OUTPUT_HPP

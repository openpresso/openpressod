#ifndef LIBOPENPRESSO_MOCK_LOGICAL_INPUT_HPP
#define LIBOPENPRESSO_MOCK_LOGICAL_INPUT_HPP

#include <gmock/gmock.h>
#include <libopenpresso/interfaces/logical_input.hpp>

class MockLogicalInput : public libopenpresso::interfaces::LogicalInput {
public:
  MOCK_METHOD(bool, getState, (), (const, override));
  MOCK_METHOD(std::optional<libopenpresso::time_delta_t>, fixedUpdateRate, (), (const, noexcept, override));
  MOCK_METHOD(libopenpresso::callback_descriptor_t, registerCallback, (const callback_t&), (override));
  MOCK_METHOD(void, unregisterCallback, (libopenpresso::callback_descriptor_t), (override));
};

#endif // LIBOPENPRESSO_MOCK_LOGICAL_INPUT_HPP

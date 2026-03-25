#ifndef LIBOPENPRESSO_MOCK_CONTROLLER_BASE_HPP
#define LIBOPENPRESSO_MOCK_CONTROLLER_BASE_HPP

#include <gmock/gmock.h>
#include <libopenpresso/interfaces/controller_base.hpp>

class MockControllerBase : public libopenpresso::interfaces::ControllerBase {
public:
  MOCK_METHOD(void, activate, (), (override));
  MOCK_METHOD(void, deactivate, (), (override));
  MOCK_METHOD(bool, isActive, (), (const, noexcept, override));
};

#endif // LIBOPENPRESSO_MOCK_CONTROLLER_BASE_HPP

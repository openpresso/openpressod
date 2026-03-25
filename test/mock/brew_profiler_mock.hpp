#ifndef LIBOPENPRESSO_MOCK_BREW_PROFILER_HPP
#define LIBOPENPRESSO_MOCK_BREW_PROFILER_HPP

#include <gmock/gmock.h>
#include <libopenpresso/interfaces/brew_profiler.hpp>

class MockBrewProfiler : public libopenpresso::interfaces::BrewProfiler {
public:
  // BrewProfiler methods
  MOCK_METHOD(void, setAutoStopCondition, (libopenpresso::brew_step_advance_conditions::OnWeight condition), (override));
  MOCK_METHOD(void, setAutoStopCondition, (libopenpresso::brew_step_advance_conditions::OnTotalTime condition), (override));
  MOCK_METHOD(void, setAutoStopCondition, (libopenpresso::brew_step_advance_conditions::Never condition), (override));

  using StepsVector_t = std::vector<std::pair<libopenpresso::step_target_t, libopenpresso::next_step_condition_t>>;
  MOCK_METHOD(void, setSteps, (const StepsVector_t& steps), (override));

  MOCK_METHOD(libopenpresso::callback_descriptor_t, registerStepChangeCallback, (const callback_t&), (override));
  MOCK_METHOD(void, unregisterStepChangeCallback, (libopenpresso::callback_descriptor_t), (override));

  // ControllerBase methods
  MOCK_METHOD(void, activate, (), (override));
  MOCK_METHOD(void, deactivate, (), (override));
  MOCK_METHOD(bool, isActive, (), (const, noexcept, override));
};

#endif // LIBOPENPRESSO_MOCK_BREW_PROFILER_HPP

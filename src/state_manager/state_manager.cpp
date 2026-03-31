#include "state_manager.hpp"

#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"

#include <ranges>
#include <utility>

#include <libopenpresso/brew_steps_data.hpp>
#include <libopenpresso/interfaces/brew_profiler.hpp>      // IWYU pragma: keep
#include <libopenpresso/interfaces/controller_base.hpp>    // IWYU pragma: keep
#include <libopenpresso/interfaces/libopenpresso_core.hpp> // IWYU pragma: keep
#include <libopenpresso/interfaces/logical_input.hpp>      // IWYU pragma: keep
#include <openpresso_proto/openpresso.pb.h>

using namespace openpressod;

StateManager::StateManager(const core_ptr_t& core, const OpenpressodConfig& config)
: m_steam{core->getLogicalInput(libopenpresso_config_labels::STEAM_BUTTON_LABEL)->getState()}
, m_brewTemperature{config.brewTemperature()}
, m_brewProfiler{core->getBrewProfiler(libopenpresso_config_labels::BREW_PROFILER_LABEL)}
, m_steamController{core->getSteamController(libopenpresso_config_labels::STEAM_CONTROLLER_LABEL)}
, m_temperatureController{core->getTemperatureController(libopenpresso_config_labels::BREW_TEMPERATURE_CONTROLLER_LABEL)}
, m_weightSensor{core->getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL)}
{
  using namespace libopenpresso::brew_step_advance_conditions;
  using namespace libopenpresso::brew_step_targets;

  auto target = ConstantPressure{config.brewPressure()};
  auto condition = Never{};
  m_brewProfiler->setSteps({{target, condition}});
}

bool StateManager::getPowerState() const noexcept
{
  return m_power;
}

void StateManager::setPowerState(bool state)
{
  if (m_power == state) {
    return;
  }

  if (m_power) {
    m_brewProfiler->deactivate();
    m_steamController->deactivate();
    m_temperatureController->deactivate();
  }
  else if (m_steam) {
    m_steamController->activate();
  }
  else {
    m_temperatureController->setTargetTemperature(m_brewTemperature);
    m_temperatureController->activate();
  }

  m_power = state;
}

bool StateManager::getBrewState() const noexcept
{
  return m_brewProfiler->isActive();
}

void StateManager::startBrew()
{
  if (m_brewProfiler->isActive()) {
    return;
  }

  if (!m_power) {
    throw std::runtime_error{"Cannot start brew while power is off"};
  }

  if (m_steam) {
    throw std::runtime_error{"Cannot start brew in steam mode"};
  }

  m_brewProfiler->activate();
}

void StateManager::stopBrew()
{
  if (!m_brewProfiler->isActive()) {
    return;
  }

  m_brewProfiler->deactivate();
}

bool StateManager::getSteamModeState() const noexcept
{
  return m_steam;
}

void StateManager::setSteamModeState(bool state)
{
  if (m_steam == state) {
    return;
  }

  if (m_brewProfiler->isActive()) {
    throw std::runtime_error{"Cannot switch to steam mode while brewing"};
  }

  if (m_power) {
    if (m_steam) {
      m_steamController->deactivate();
      m_temperatureController->setTargetTemperature(m_brewTemperature);
      m_temperatureController->activate();
    }
    else {
      m_temperatureController->deactivate();
      m_steamController->activate();
    }
  }

  m_steam = state;
}

void StateManager::resetScales()
{
  if (m_brewProfiler->isActive()) {
    throw std::runtime_error{"Cannot reset scale while brewing"};
  }
  m_weightSensor->tare();
}

void StateManager::setBrewProfile(const openpresso::BrewProfile* profile)
{
  if (m_brewProfiler->isActive()) {
    throw std::runtime_error{"Cannot change profile while brewing"};
  }

  m_brewTemperature = profile->temperature();
  if (!m_steam) {
    m_temperatureController->setTargetTemperature(m_brewTemperature);
  }

  constexpr auto stepTransformer = [](const openpresso::BrewStep& step) {
    return std::make_pair(getStepTarget(step), getStepCondition(step));
  };
  auto steps = profile->steps() | std::views::transform(stepTransformer);
  m_brewProfiler->setSteps({steps.begin(), steps.end()});

  setAutoStopCondition(profile);
}

libopenpresso::step_target_t StateManager::getStepTarget(const openpresso::BrewStep& step)
{
  using namespace libopenpresso::brew_step_targets;

  if (step.has_pressure()) {
    return ConstantPressure{step.pressure()};
  }

  if (step.has_flowrate()) {
    return ConstantFlow{step.flowrate()};
  }

  throw std::runtime_error{"Brew step has no target"};
}

libopenpresso::next_step_condition_t StateManager::getStepCondition(const openpresso::BrewStep& step)
{
  using namespace libopenpresso::brew_step_advance_conditions;

  if (step.has_steptime()) {
    return OnStepTime{std::chrono::nanoseconds{step.steptime().nanos()}};
  }

  if (step.has_totaltime()) {
    return OnTotalTime{std::chrono::nanoseconds{step.totaltime().nanos()}};
  }

  if (step.has_totalweight()) {
    return OnWeight{step.totalweight()};
  }

  return Never{};
}

void StateManager::setAutoStopCondition(const openpresso::BrewProfile* profile)
{
  using namespace libopenpresso::brew_step_advance_conditions;
  if (profile->has_totaltime()) {
    auto time = std::chrono::nanoseconds{profile->totaltime().nanos()};
    m_brewProfiler->setAutoStopCondition(OnTotalTime{time});
  }
  else if (profile->has_totalweight()) {
    m_brewProfiler->setAutoStopCondition(OnWeight{profile->totalweight()});
  }
  else {
    m_brewProfiler->setAutoStopCondition(Never{});
  }
}

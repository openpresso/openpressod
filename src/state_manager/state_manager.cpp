#include "state_manager.hpp"

#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"
#include "leds/leds_handler_interface.hpp"
#include "service/events_stream_reactor.hpp"

#include <farmhash.h>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <utility>

#include <libopenpresso/brew_steps_data.hpp>
#include <libopenpresso/interfaces/brew_profiler.hpp>      // IWYU pragma: keep
#include <libopenpresso/interfaces/controller_base.hpp>    // IWYU pragma: keep
#include <libopenpresso/interfaces/libopenpresso_core.hpp> // IWYU pragma: keep
#include <libopenpresso/interfaces/logical_input.hpp>      // IWYU pragma: keep
#include <openpresso_proto/openpresso.pb.h>
#include <spdlog/spdlog.h>

using namespace openpressod;

StateManager::StateManager(const core_ptr_t& core,
                           const OpenpressodConfig& config,
                           std::unique_ptr<LedsHandlerInterface>&& leds)
: m_steam{core->getLogicalInput(libopenpresso_config_labels::STEAM_BUTTON_LABEL)->getState()}
, m_brewTemperature{config.brewTemperature()}
, m_brewProfiler{core->getBrewProfiler(libopenpresso_config_labels::BREW_PROFILER_LABEL)}
, m_steamController{core->getSteamController(libopenpresso_config_labels::STEAM_CONTROLLER_LABEL)}
, m_temperatureController{core->getTemperatureController(libopenpresso_config_labels::BREW_TEMPERATURE_CONTROLLER_LABEL)}
, m_weightSensor{core->getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL)}
, m_leds(std::move(leds))
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
    m_leds->indicatePowerOff();
  }
  else if (m_steam) {
    m_steamController->activate();
    m_leds->indicateSteamState();
  }
  else {
    m_temperatureController->setTargetTemperature(m_brewTemperature);
    m_temperatureController->activate();
    m_leds->indicateBrewState(m_brewTemperature);
  }

  m_power = state;

  PowerState stateChange;
  stateChange.set_value(state);
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(stateChange);
  }
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

  BrewProgress noProgress;
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(noProgress);
  }
}

void openpressod::StateManager::onBrewStepChange(size_t index)
{
  BrewProgress progress;
  progress.set_brewstepindex(index);
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(progress);
  }
}

bool StateManager::getSteamModeState() const noexcept
{
  spdlog::info("Steam mode state requsted, state: {}", m_steam ? "ON" : "OFF");
  return m_steam;
}

void StateManager::setSteamModeState(bool state)
{
  if (m_steam == state) {
    spdlog::warn("Steam mode is already {}", m_steam ? "ON" : "OFF");
    return;
  }

  spdlog::info("Steam mode state change from {} to: {}", m_steam ? "ON" : "OFF", state ? "ON" : "OFF");

  if (m_brewProfiler->isActive()) {
    throw std::runtime_error{"Cannot switch to steam mode while brewing"};
  }

  if (m_power) {
    if (m_steam) {
      m_steamController->deactivate();
      m_temperatureController->setTargetTemperature(m_brewTemperature);
      m_temperatureController->activate();
      m_leds->indicateBrewState(m_brewTemperature);
    }
    else {
      m_temperatureController->deactivate();
      m_steamController->activate();
      m_leds->indicateSteamState();
    }
  }

  m_steam = state;

  SteamModeState stateChange;
  stateChange.set_isactive(state);
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(stateChange);
  }
}

void StateManager::resetScales()
{
  if (m_brewProfiler->isActive()) {
    throw std::runtime_error{"Cannot reset scale while brewing"};
  }
  m_weightSensor->tare();
}

void StateManager::setBrewProfile(const BrewProfile* profile)
{
  if (m_brewProfiler->isActive()) {
    throw std::runtime_error{"Cannot change profile while brewing"};
  }

  if (profile->name().empty()) {
    throw std::runtime_error{"Profile name cannot be empty"};
  }

  auto hash = makeProfileHash(profile);

  m_brewTemperature = profile->temperature();
  if (!m_steam && m_power) {
    m_temperatureController->setTargetTemperature(m_brewTemperature);
    m_leds->indicateBrewState(m_brewTemperature);
  }

  constexpr auto stepTransformer = [](const BrewStep& step) {
    return std::make_pair(getStepTarget(step), getStepCondition(step));
  };
  auto steps = profile->steps() | std::views::transform(stepTransformer);
  m_brewProfiler->setSteps({steps.begin(), steps.end()});

  setAutoStopCondition(profile);

  m_brewProfileName = profile->name();
  m_brewProfileHash = std::move(hash);

  BrewProfileInfo info;
  info.set_name(m_brewProfileName);
  info.set_hash(m_brewProfileHash);
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(info);
  }
}

libopenpresso::step_target_t StateManager::getStepTarget(const BrewStep& step)
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

libopenpresso::next_step_condition_t StateManager::getStepCondition(const BrewStep& step)
{
  using namespace libopenpresso::brew_step_advance_conditions;

  if (step.has_steptime()) {
    return OnStepTime{std::chrono::seconds{step.steptime().seconds()} +
                      std::chrono::nanoseconds{step.steptime().nanos()}};
  }

  if (step.has_totaltime()) {
    return OnTotalTime{std::chrono::seconds{step.totaltime().seconds()} +
                       std::chrono::nanoseconds{step.totaltime().nanos()}};
  }

  if (step.has_totalweight()) {
    return OnWeight{step.totalweight()};
  }

  return Never{};
}

void StateManager::setAutoStopCondition(const BrewProfile* profile)
{
  using namespace libopenpresso::brew_step_advance_conditions;

  if (profile->has_totaltime()) {
    auto time = std::chrono::seconds{profile->totaltime().seconds()} +
                std::chrono::nanoseconds{profile->totaltime().nanos()};
    m_brewProfiler->setAutoStopCondition(OnTotalTime{time});
  }
  else if (profile->has_totalweight()) {
    m_brewProfiler->setAutoStopCondition(OnWeight{profile->totalweight()});
  }
  else {
    m_brewProfiler->setAutoStopCondition(Never{});
  }
}

const std::string& StateManager::brewProfileName() const noexcept
{
  return m_brewProfileName;
}

const std::string& StateManager::brewProfileHash() const noexcept
{
  return m_brewProfileHash;
}

std::string StateManager::makeProfileHash(const BrewProfile* profile)
{
  std::string serialized;
  google::protobuf::io::StringOutputStream out(&serialized);
  google::protobuf::io::CodedOutputStream coded(&out);
  coded.SetSerializationDeterministic(true);
  if (!profile->SerializeToCodedStream(&coded)) {
    throw std::runtime_error{"failed to calculate profile hash"};
  }
  auto fingerprint = util::Fingerprint128(serialized);
  return std::format("{:016x}{:016x}", fingerprint.first, fingerprint.second);
}

void StateManager::addEventsStreamReactor(std::unique_ptr<EventsStreamReactor> reactor)
{
  m_eventsSinks.push_back(std::move(reactor));
}

void StateManager::releaseEventsStreamReactor(const EventsStreamReactor* reactor)
{
  m_eventsSinks.remove_if([reactor](auto&& val) { return val.get() == reactor; });
}

void openpressod::StateManager::unregisterBrewProfileCallback(libopenpresso::callback_descriptor_t descr)
{
  m_brewProfiler->unregisterStepChangeCallback(descr);
}

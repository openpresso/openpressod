#include "state_manager.hpp"

#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"
#include "leds/leds_handler_interface.hpp"
#include "service/events_stream_reactor.hpp"

#include <exception>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <xxhash.h>

#include <google/protobuf/util/json_util.h>
#include <libopenpresso/brew_steps_data.hpp>
#include <libopenpresso/interfaces/brew_profiler.hpp>      // IWYU pragma: keep
#include <libopenpresso/interfaces/controller_base.hpp>    // IWYU pragma: keep
#include <libopenpresso/interfaces/libopenpresso_core.hpp> // IWYU pragma: keep
#include <libopenpresso/interfaces/logical_input.hpp>      // IWYU pragma: keep
#include <openpresso_proto/openpresso.pb.h>
#include <spdlog/fmt/bin_to_hex.h>
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
, m_leds{std::move(leds)}
, m_brewProfilePath{config.brewProfilePath()}
{
  using namespace libopenpresso::brew_step_advance_conditions;
  using namespace libopenpresso::brew_step_targets;

  try {
    restoreProfile();
  }
  catch (const std::exception& e) {
    spdlog::warn("Failed to restore brew profile from json: {}", e.what());
    spdlog::info("Fallback to constant {} millibars pressure mode", config.brewPressure());

    auto target = ConstantPressure{config.brewPressure()};
    auto condition = Never{};
    m_brewProfiler->setSteps({{target, condition}});
    m_brewProfiler->setAutoStopCondition(Never{});
  }
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
  applyProfile(profile);
  saveProfile(profile);

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

uint64_t StateManager::brewProfileHash() const noexcept
{
  return m_brewProfileHash;
}

uint64_t StateManager::makeProfileHash(const BrewProfile* profile)
{
  std::string serialized;
  {
    google::protobuf::io::StringOutputStream out(&serialized);
    google::protobuf::io::CodedOutputStream coded(&out);
    coded.SetSerializationDeterministic(true);
    if (!profile->SerializeToCodedStream(&coded)) {
      throw std::runtime_error{"failed to calculate profile hash"};
    }
  }

  spdlog::trace("Brew profile serialized for hash:{}", spdlog::to_hex(serialized));
  return XXH64(serialized.data(), serialized.size(), {});
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

void openpressod::StateManager::applyProfile(const BrewProfile* profile)
{
  if (m_brewProfiler->isActive()) {
    throw std::runtime_error{"Cannot change profile while brewing"};
  }

  if (profile->name().empty()) {
    throw std::runtime_error{"Profile name cannot be empty"};
  }

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
  m_brewProfileHash = makeProfileHash(profile);
  spdlog::info("Brew profile \"{}\" applied, hash: 0x{:016x}", m_brewProfileName, m_brewProfileHash);
}

void openpressod::StateManager::restoreProfile()
{
  std::ifstream inputFile{m_brewProfilePath};
  if (!inputFile.is_open()) {
    throw std::runtime_error(std::format("Failed to open a file to read a brew profile: {}",
                                         m_brewProfilePath.string()));
  }

  std::string content{std::istreambuf_iterator<std::string::value_type>(inputFile),
                      std::istreambuf_iterator<std::string::value_type>()};

  if (inputFile.fail()) {
    throw std::runtime_error(std::format("Failed to read brew profile from file: {}",
                                         m_brewProfilePath.string()));
  }

  spdlog::trace("Brew profile restored:\n{}", content);

  BrewProfile profile;

  auto status = google::protobuf::util::JsonStringToMessage(content, &profile);
  if (!status.ok()) {
    throw std::runtime_error{std::format("Failed to deserialize brew profile: {}", status.message())};
  }

  applyProfile(&profile);
}

void StateManager::saveProfile(const BrewProfile* profile)
{
  std::string jsonOutput;
  google::protobuf::util::JsonPrintOptions options = {
    .add_whitespace = true,
    .always_print_fields_with_no_presence = true,
    .preserve_proto_field_names = true,
  };

  auto status = google::protobuf::util::MessageToJsonString(*profile, &jsonOutput, options);
  if (!status.ok()) {
    throw std::runtime_error{std::format("Failed to serialize brew profile: {}", status.message())};
  }

  std::ofstream outputFile{m_brewProfilePath};
  if (!outputFile.is_open()) {
    throw std::runtime_error(std::format("Failed to open a file to write a brew profile: {}",
                                         m_brewProfilePath.string()));
  }

  outputFile << jsonOutput;

  if (outputFile.fail()) {
    throw std::runtime_error(std::format("Failed to write brew profile to file: {}",
                                         m_brewProfilePath.string()));
  }
}

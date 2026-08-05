#include "brew_profile_manager.hpp"

#include "config/openpressod_config.hpp"
#include "state_manager.hpp"

#include <chrono>
#include <format>
#include <fstream>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>

#include <google/protobuf/util/json_util.h>
#include <libopenpresso/brew_steps_data.hpp>
#include <libopenpresso/types.hpp>
#include <openpresso_proto/openpresso.pb.h>
#include <spdlog/spdlog.h>

using namespace openpressod;

BrewProfileManager::BrewProfileManager(const OpenpressodConfig& config)
: m_profilePath{config.brewProfilePath()}
, m_defaultProfile{makeDefaultProfile(config)}
{
}

BrewProfile BrewProfileManager::makeDefaultProfile(const OpenpressodConfig& config)
{
  BrewProfile defaultProfile;
  defaultProfile.set_temperature(config.brewTemperature());
  BrewStep step;
  step.set_targetvalue(static_cast<float>(config.brewPressure()));
  step.set_targettype(BrewStepTargetType::BrewStepTargetType_Pressure);
  defaultProfile.mutable_steps()->Add(std::move(step));
  return defaultProfile;
}

void BrewProfileManager::loadSavedProfile()
{
  std::ifstream inputFile{m_profilePath};
  if (!inputFile.is_open()) {
    throw std::runtime_error(std::format("Failed to open a file to read a brew profile: {}",
                                         m_profilePath.string()));
  }

  std::string content{std::istreambuf_iterator<std::string::value_type>(inputFile),
                      std::istreambuf_iterator<std::string::value_type>()};

  if (inputFile.fail()) {
    throw std::runtime_error(std::format("Failed to read brew profile from file: {}",
                                         m_profilePath.string()));
  }

  spdlog::trace("Brew profile content:\n{}", content);

  auto status = google::protobuf::util::JsonStringToMessage(content, &m_currentProfile);
  if (!status.ok()) {
    throw std::runtime_error{std::format("Failed to deserialize brew profile: {}", status.message())};
  }

  spdlog::debug("Brew profile \"{}\" loaded from file: {}", m_currentProfile.name(), m_profilePath.string());
}

void BrewProfileManager::loadDefaultProfile()
{
  m_currentProfile.CopyFrom(m_defaultProfile);
  spdlog::info("Default profile loaded");
}

void BrewProfileManager::saveProfile(const BrewProfile& profile)
{
  m_currentProfile.CopyFrom(profile);

  std::string jsonOutput;
  google::protobuf::util::JsonPrintOptions options = {
    .add_whitespace = true,
    .always_print_fields_with_no_presence = true,
    .preserve_proto_field_names = true,
  };

  auto status = google::protobuf::util::MessageToJsonString(profile, &jsonOutput, options);
  if (!status.ok()) {
    throw std::runtime_error{std::format("Failed to serialize brew profile: {}", status.message())};
  }

  spdlog::trace("Brew profile serialized:\n{}", jsonOutput);

  std::ofstream outputFile{m_profilePath};
  if (!outputFile.is_open()) {
    throw std::runtime_error(std::format("Failed to open a file to write a brew profile: {}",
                                         m_profilePath.string()));
  }

  outputFile << jsonOutput;

  if (outputFile.fail()) {
    throw std::runtime_error(std::format("Failed to write brew profile to file: {}",
                                         m_profilePath.string()));
  }
}

const BrewProfile& BrewProfileManager::getProfile() const noexcept
{
  return m_currentProfile;
}

void BrewProfileManager::applyBrewProfile(StateManager& stateManager) const
{
  stateManager.setBrewTemperature(m_currentProfile.temperature());

  constexpr auto stepTransformer = [](const BrewStep& step) {
    return std::make_pair(getStepTarget(step), getStepCondition(step));
  };

  stateManager.setBrewSteps(m_currentProfile.steps() | std::views::transform(stepTransformer));

  setAutoStopCondition(stateManager);

  if (m_currentProfile.name().empty()) {
    spdlog::warn("Unnamed brew profile applied");
  }
  else {
    spdlog::info("Brew profile \"{}\" applied", m_currentProfile.name());
  }
}

libopenpresso::step_target_t BrewProfileManager::getStepTarget(const BrewStep& step)
{
  using namespace libopenpresso::brew_step_targets;

  switch (step.targettype()) {
  case BrewStepTargetType_Pressure:
    return ConstantPressure{static_cast<libopenpresso::millibars_t>(step.targetvalue())};
  case BrewStepTargetType_FlowRate:
    return ConstantFlow{static_cast<libopenpresso::milligrams_p_second_t>(step.targetvalue())};
  default:
    throw std::runtime_error{"Brew step has no target"};
  }
}

libopenpresso::next_step_condition_t BrewProfileManager::getStepCondition(const BrewStep& step)
{
  using namespace libopenpresso::brew_step_advance_conditions;

  if (!step.has_advancecondition()) {
    return Never{};
  }

  switch (step.advancecondition().type()) {
  case BrewStepAdvanceConditionType_TotalTime:
    return OnTotalTime{ToDuration(step.advancecondition().value())};
  case BrewStepAdvanceConditionType_TotalWeight:
    return OnWeight{ToWeight(step.advancecondition().value())};
  case BrewStepAdvanceConditionType_StepTime:
    return OnStepTime{ToDuration(step.advancecondition().value())};
  default:
    throw std::runtime_error{"Unknow advance condition"};
  }
}

void BrewProfileManager::setAutoStopCondition(StateManager& stateManager) const
{
  using namespace libopenpresso::brew_step_advance_conditions;

  if (!m_currentProfile.has_stopcondition()) {
    stateManager.setAutoStopCondition(Never{});
    return;
  }

  switch (m_currentProfile.stopcondition().type()) {
  case StopConditionType_TotalTime:
    stateManager.setAutoStopCondition(OnTotalTime{ToDuration(m_currentProfile.stopcondition().value())});
    break;
  case StopConditionType_TotalWeight:
    stateManager.setAutoStopCondition(OnWeight{ToWeight(m_currentProfile.stopcondition().value())});
    break;
  default:
    throw std::runtime_error{"Unknow stop condition"};
  }
}

libopenpresso::time_delta_t openpressod::BrewProfileManager::ToDuration(float value)
{
  using fp_seconds = std::chrono::duration<float>;
  return std::chrono::duration_cast<libopenpresso::time_delta_t>(fp_seconds{value});
}

libopenpresso::milligrams_t openpressod::BrewProfileManager::ToWeight(float value)
{
  return static_cast<libopenpresso::milligrams_t>(value);
}

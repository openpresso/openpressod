#include "brew_profile_manager.hpp"

#include "config/openpressod_config.hpp"

#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

#include <google/protobuf/util/json_util.h>
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
  step.set_pressure(config.brewPressure());
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

  spdlog::info("Brew profile \"{}\" loaded from file: {}", m_currentProfile.name(), m_profilePath.string());
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

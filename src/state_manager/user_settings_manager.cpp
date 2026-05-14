#include "user_settings_manager.hpp"

#include "config/openpressod_config.hpp"
#include "state_manager/state_manager.hpp"

#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <google/protobuf/util/json_util.h>
#include <openpresso_proto/openpresso.pb.h>
#include <spdlog/spdlog.h>

using namespace openpressod;

UserSettingsManager::UserSettingsManager(const OpenpressodConfig& config)
: m_settingsPath(config.userSettingsPath())
, m_defaultSettings(makeDefaultSettings(config))
{
}

void UserSettingsManager::loadSavedSettings()
{
  std::ifstream inputFile{m_settingsPath};
  if (!inputFile.is_open()) {
    throw std::runtime_error(std::format("Failed to open a file to read user settings: {}",
                                         m_settingsPath.string()));
  }

  std::string content{std::istreambuf_iterator<std::string::value_type>(inputFile),
                      std::istreambuf_iterator<std::string::value_type>()};

  if (inputFile.fail()) {
    throw std::runtime_error(std::format("Failed to read user settings from file: {}",
                                         m_settingsPath.string()));
  }

  spdlog::trace("User settings content:\n{}", content);

  auto status = google::protobuf::util::JsonStringToMessage(content, &m_currentSettings);
  if (!status.ok()) {
    throw std::runtime_error{std::format("Failed to deserialize user settings: {}", status.message())};
  }

  spdlog::debug("User settings loaded from file: {}", m_settingsPath.string());
}

void UserSettingsManager::loadDefaultSettings()
{
  m_currentSettings.CopyFrom(m_defaultSettings);
  spdlog::info("Default user settings loaded");
}

void UserSettingsManager::saveSettings(const UserSettings& settings)
{
  m_currentSettings.CopyFrom(settings);

  std::string jsonOutput;
  google::protobuf::util::JsonPrintOptions options = {
    .add_whitespace = true,
    .always_print_fields_with_no_presence = true,
    .preserve_proto_field_names = true,
  };

  auto status = google::protobuf::util::MessageToJsonString(settings, &jsonOutput, options);
  if (!status.ok()) {
    throw std::runtime_error{std::format("Failed to serialize user settings: {}", status.message())};
  }

  spdlog::trace("Brew profile serialized:\n{}", jsonOutput);

  std::ofstream outputFile{m_settingsPath};
  if (!outputFile.is_open()) {
    throw std::runtime_error(std::format("Failed to open a file to write user settings: {}",
                                         m_settingsPath.string()));
  }

  outputFile << jsonOutput;

  if (outputFile.fail()) {
    throw std::runtime_error(std::format("Failed to write user settings to file: {}",
                                         m_settingsPath.string()));
  }
}

const UserSettings& UserSettingsManager::getSettings() const noexcept
{
  return m_currentSettings;
}

UserSettings UserSettingsManager::makeDefaultSettings(const OpenpressodConfig& config)
{
  UserSettings settings;
  settings.set_steamttemperature(config.steamTemperature());
  return settings;
}

void UserSettingsManager::applySettings(StateManager& stateManager)
{
  stateManager.setSteamTemperature(m_currentSettings.steamttemperature());
  spdlog::info("User settings applied");
}

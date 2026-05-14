#ifndef USER_SETTINGS_MANAGER_HPP
#define USER_SETTINGS_MANAGER_HPP

#include <filesystem>

#include <openpresso_proto/openpresso.pb.h>

namespace openpressod
{

class OpenpressodConfig;
class StateManager;

class UserSettingsManager {
public:
  UserSettingsManager(const OpenpressodConfig& config);
  void loadSavedSettings();
  void loadDefaultSettings();
  void saveSettings(const UserSettings& settings);
  const UserSettings& getSettings() const noexcept;
  void applySettings(StateManager& stateManager);

private:
  static UserSettings makeDefaultSettings(const OpenpressodConfig& config);

private:
  std::filesystem::path m_settingsPath;
  UserSettings m_currentSettings;
  UserSettings m_defaultSettings;
};

} // namespace openpressod

#endif // USER_SETTINGS_MANAGER_HPP
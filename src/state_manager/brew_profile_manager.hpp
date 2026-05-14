#ifndef BREW_PROFILE_MANAGER_HPP
#define BREW_PROFILE_MANAGER_HPP

#include <filesystem>

#include <libopenpresso/brew_steps_data.hpp>
#include <openpresso_proto/openpresso.pb.h>

namespace openpressod
{

class OpenpressodConfig;
class StateManager;

class BrewProfileManager {
public:
  BrewProfileManager(const OpenpressodConfig& config);
  void loadSavedProfile();
  void loadDefaultProfile();
  void saveProfile(const BrewProfile& profile);
  const BrewProfile& getProfile() const noexcept;

  void applyBrewProfile(StateManager& stateManager) const;

private:
  static BrewProfile makeDefaultProfile(const OpenpressodConfig& config);
  static libopenpresso::step_target_t getStepTarget(const BrewStep& step);
  static libopenpresso::next_step_condition_t getStepCondition(const BrewStep& step);
  void setAutoStopCondition(StateManager& stateManager) const;

private:
  std::filesystem::path m_profilePath;
  BrewProfile m_currentProfile;
  BrewProfile m_defaultProfile;
};

} // namespace openpressod

#endif // BREW_PROFILE_MANAGER_HPP
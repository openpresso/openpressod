#ifndef BREW_PROFILE_MANAGER_HPP
#define BREW_PROFILE_MANAGER_HPP

#include <filesystem>

#include <openpresso_proto/openpresso.pb.h>

namespace openpressod
{

class OpenpressodConfig;

class BrewProfileManager {
public:
  BrewProfileManager(const OpenpressodConfig& config);
  void loadSavedProfile();
  void loadDefaultProfile();
  void saveProfile(const BrewProfile& profile);
  const BrewProfile& getProfile() const noexcept;

private:
  static BrewProfile makeDefaultProfile(const OpenpressodConfig& config);

private:
  std::filesystem::path m_profilePath;
  BrewProfile m_currentProfile;
  BrewProfile m_defaultProfile;
};

} // namespace openpressod

#endif // BREW_PROFILE_MANAGER_HPP
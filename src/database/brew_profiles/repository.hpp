#ifndef BREW_PROFILES_REPOSITORY_HPP
#define BREW_PROFILES_REPOSITORY_HPP

#include "schema.hpp"

#include <cstdint>
#include <string>

#include <openpresso_proto/brew_profile.pb.h>

namespace openpressod::db::brew_profiles
{

class BrewProfilesRepository {
public:
  explicit BrewProfilesRepository(std::string path);

  uint64_t create(const BrewProfile& profile);
  void update(uint64_t id, const BrewProfile& profile);
  PersistentBrewProfile get(uint64_t id);
  void erase(uint64_t id);
  PersistentBrewProfile find(const BrewProfile& profile);
  BrewProfilesSearchResult find(const FindBrewProfiles& query);

private:
  static ProfileRow makeProfileRow(uint64_t id, const BrewProfile& profile);
  static uint64_t nowSeconds();
  void saveSteps(uint64_t id, const BrewProfile& profile);
  void loadSteps(uint64_t id, BrewProfile& profile);
  static void toPersistentProfile(const ProfileRow& row, PersistentBrewProfile& profile);

private:
  Storage m_storage;
};

} // namespace openpressod::db::brew_profiles

#endif // BREW_PROFILES_REPOSITORY_HPP
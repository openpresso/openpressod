#ifndef BREW_PROFILES_REPOSITORY_HPP
#define BREW_PROFILES_REPOSITORY_HPP

#include "database/database_id.hpp"
#include "schema.hpp"

#include <cstdint>
#include <string>

#include <openpresso_proto/brew_profile.pb.h>

namespace openpressod::db::brew_profiles
{

class BrewProfilesRepository {
public:
  explicit BrewProfilesRepository(std::string path);
  BrewProfilesRepository(BrewProfilesRepository&&) = delete;
  BrewProfilesRepository(const BrewProfilesRepository&) = default;
  auto operator=(const BrewProfilesRepository&) = delete;
  auto operator=(BrewProfilesRepository&&) = delete;
  ~BrewProfilesRepository() = default;

  DatabaseId create(const BrewProfile& profile);
  void update(DatabaseId id, const BrewProfile& profile);
  PersistentBrewProfile get(DatabaseId id);
  // Throws when the ID is missing.
  void erase(DatabaseId id);
  PersistentBrewProfile find(const BrewProfile& profile);
  BrewProfilesSearchResult find(const FindBrewProfiles& query);

private:
  static ProfileRow makeProfileRow(DatabaseId id, const BrewProfile& profile);
  static uint64_t nowSeconds();
  void saveSteps(DatabaseId id, const BrewProfile& profile);
  void loadSteps(DatabaseId id, BrewProfile& profile);
  static void toPersistentProfile(const ProfileRow& row, PersistentBrewProfile& profile);

private:
  Storage m_storage;
};

} // namespace openpressod::db::brew_profiles

#endif // BREW_PROFILES_REPOSITORY_HPP
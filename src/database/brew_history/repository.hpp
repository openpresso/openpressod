#ifndef BREW_HISTORY_REPOSITORY_HPP
#define BREW_HISTORY_REPOSITORY_HPP

#include "brew_history/brew_history_data.hpp"
#include "database/database_id.hpp"
#include "schema.hpp"

#include <string>

#include <openpresso_proto/brew_history.pb.h>
#include <openpresso_proto/brew_profile.pb.h>

namespace openpressod::db::brew_history
{

class BrewHistoryRepository {
public:
  explicit BrewHistoryRepository(std::string path);
  BrewHistoryRepository(BrewHistoryRepository&&) = delete;
  BrewHistoryRepository(const BrewHistoryRepository&) = default;
  auto operator=(const BrewHistoryRepository&) = delete;
  auto operator=(BrewHistoryRepository&&) = delete;
  ~BrewHistoryRepository() = default;

  // Reserve a persistent ID; incomplete entries are excluded from get() and find().
  DatabaseId createEmptyEntry();
  void completeEntry(DatabaseId id, const CompletedBrewHistoryEntry& entry);
  CompletedBrewHistoryEntry get(DatabaseId id);
  void erase(DatabaseId id);
  void discardIncomplete();
  void discardIncomplete(DatabaseId id);
  BrewHistorySearchResult find(const FindBrewHistory& query);

private:
  DatabaseId saveProfile(const BrewProfile& profile);
  BrewProfile loadProfile(DatabaseId id);
  void saveSteps(DatabaseId id, const BrewProfile& profile);
  void loadSteps(DatabaseId id, BrewProfile& profile);
  void saveSamples(DatabaseId id, const CompletedBrewHistoryEntry& entry);
  CompletedBrewHistoryEntry loadEntry(const BrewHistoryEntryRow& row);

private:
  Storage m_storage;
};

} // namespace openpressod::db::brew_history

#endif // BREW_HISTORY_REPOSITORY_HPP

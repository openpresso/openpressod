#ifndef BREW_HISTORY_REPOSITORY_HPP
#define BREW_HISTORY_REPOSITORY_HPP

#include "brew_log/brew_log_data.hpp"
#include "schema.hpp"

#include <cstdint>
#include <string>
#include <utility>

#include <openpresso_proto/brew_log.pb.h>
#include <openpresso_proto/brew_profile.pb.h>

namespace openpressod::db::brew_history
{

class BrewHistoryRepository {
public:
  explicit BrewHistoryRepository(std::string path);

  // Reserve a persistent ID; incomplete logs are excluded from get() and find().
  uint64_t createEmptyLog();
  void completeLog(uint64_t id, const CompletedBrewLog& log, const BrewProfile& profile);
  std::pair<CompletedBrewLog, BrewProfile> get(uint64_t id);
  void erase(uint64_t id);
  void discardIncomplete();
  void discardIncomplete(uint64_t id);
  BrewLogsSearchResult find(const FindBrewLogs& query);

private:
  uint64_t saveProfile(const BrewProfile& profile);
  BrewProfile loadProfile(uint64_t id);
  void saveSteps(uint64_t id, const BrewProfile& profile);
  void loadSteps(uint64_t id, BrewProfile& profile);
  void saveRecords(uint64_t id, const CompletedBrewLog& log);
  CompletedBrewLog loadLog(const BrewLogRow& row);

private:
  Storage m_storage;
};

} // namespace openpressod::db::brew_history

#endif // BREW_HISTORY_REPOSITORY_HPP

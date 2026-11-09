#ifndef BREW_HISTORY_SCHEMA_HPP
#define BREW_HISTORY_SCHEMA_HPP

#include "database/database_id.hpp"
#include "database/steps_schema.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <openpresso_proto/brew_profile.pb.h>
#include <sqlite_orm/sqlite_orm.h>

namespace openpressod::db::brew_history
{

struct BrewHistoryEntryRow {
  DatabaseId id = 0;
  int64_t startTime = 0;        // Milliseconds since the Unix epoch.
  int64_t samplingInterval = 0; // Milliseconds.
  std::optional<DatabaseId> profileId;
};

struct HistoricalProfileRow {
  DatabaseId id = 0;
  std::string name;
  int32_t temperature = 0;
  std::optional<StopConditionType> stopType;
  std::optional<float> stopValue;
  uint64_t fingerprint = 0;
};

struct BrewHistoryEntrySampleRow {
  DatabaseId brewId = 0;
  uint32_t position = 0;
  uint32_t pressure = 0;
  int32_t weight = 0;
  int32_t flowRate = 0;
  uint32_t step = 0;
};

inline auto makeBrewHistoryEntryTable()
{
  using namespace sqlite_orm;

  return make_table(
    "brew_history_entries",
    make_column("id", &BrewHistoryEntryRow::id, primary_key().autoincrement()),
    make_column("start", &BrewHistoryEntryRow::startTime),
    make_column("sampling_interval", &BrewHistoryEntryRow::samplingInterval),
    make_column("profile", &BrewHistoryEntryRow::profileId),

    foreign_key(&BrewHistoryEntryRow::profileId).references(&HistoricalProfileRow::id).on_delete.restrict_());
}

inline auto makeBrewHistoryEntrySampleTable()
{
  using namespace sqlite_orm;

  return make_table(
    "brew_history_entry_samples",
    make_column("brew_id", &BrewHistoryEntrySampleRow::brewId),
    make_column("position", &BrewHistoryEntrySampleRow::position),
    make_column("pressure", &BrewHistoryEntrySampleRow::pressure),
    make_column("weight", &BrewHistoryEntrySampleRow::weight),
    make_column("flowRate", &BrewHistoryEntrySampleRow::flowRate),
    make_column("step", &BrewHistoryEntrySampleRow::step),

    primary_key(&BrewHistoryEntrySampleRow::brewId, &BrewHistoryEntrySampleRow::position),
    foreign_key(&BrewHistoryEntrySampleRow::brewId).references(&BrewHistoryEntryRow::id).on_delete.cascade());
}

inline auto makeHistoricalProfilesTable()
{
  using namespace sqlite_orm;

  return make_table("profiles",
                    make_column("id", &HistoricalProfileRow::id, primary_key().autoincrement()),
                    make_column("name", &HistoricalProfileRow::name),
                    make_column("temperature", &HistoricalProfileRow::temperature),
                    make_column("stop_type", &HistoricalProfileRow::stopType),
                    make_column("stop_value", &HistoricalProfileRow::stopValue),
                    make_column("fingerprint", &HistoricalProfileRow::fingerprint, unique()),

                    check(or_(and_(is_null(&HistoricalProfileRow::stopType),
                                   is_null(&HistoricalProfileRow::stopValue)),
                              and_(is_not_null(&HistoricalProfileRow::stopType),
                                   is_not_null(&HistoricalProfileRow::stopValue)))));
}

inline auto makeStorage(std::string filename)
{
  using namespace sqlite_orm;

  // sqlite_orm synchronizes schema objects in reverse declaration order.
  // Create parent tables before child tables, and tables before their indexes.
  return make_storage(std::move(filename),
                      make_index("ix_brew_history_entries_start", &BrewHistoryEntryRow::startTime),
                      make_index("ix_brew_history_entries_profile", &BrewHistoryEntryRow::profileId),
                      make_index("ix_historical_profiles_name_like",
                                 indexed_column(&HistoricalProfileRow::name).collate("NOCASE")),
                      make_index("ix_historical_profiles_fingerprint", &HistoricalProfileRow::fingerprint),
                      makeBrewHistoryEntrySampleTable(),
                      makeBrewHistoryEntryTable(),
                      makeStepsTable<HistoricalProfileRow>(),
                      makeHistoricalProfilesTable());
}

using Storage = decltype(makeStorage({}));

} // namespace openpressod::db::brew_history

#endif // BREW_HISTORY_SCHEMA_HPP

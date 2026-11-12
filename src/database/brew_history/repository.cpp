#include "repository.hpp"

#include "brew_history/brew_history_data.hpp"
#include "database/brew_profile_fingerprint.hpp"
#include "database/database_id.hpp"
#include "database/steps_schema.hpp"
#include "schema.hpp"
#include "service/protobuf_helpers.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>

#include <google/protobuf/util/time_util.h>
#include <openpresso_proto/brew_history.pb.h>
#include <openpresso_proto/brew_profile.pb.h>
#include <sqlite_orm/sqlite_orm.h>

using namespace openpressod::db::brew_history;

BrewHistoryRepository::BrewHistoryRepository(std::string path)
: m_storage(makeStorage(std::move(path)))
{
  m_storage.open_forever();
  m_storage.sync_schema();
}

openpressod::DatabaseId BrewHistoryRepository::createEmptyEntry()
{
  auto transaction = m_storage.immediate_transaction_guard();

  BrewHistoryEntryRow row;
  m_storage.insert(row);
  // sqlite_orm::insert returns int; retain SQLite's full 64-bit row ID.
  row.id = static_cast<DatabaseId>(m_storage.last_insert_rowid());

  transaction.commit();
  return row.id;
}

void BrewHistoryRepository::completeEntry(DatabaseId id, const CompletedBrewHistoryEntry& entry)
{
  auto transaction = m_storage.immediate_transaction_guard();

  auto row = m_storage.get<BrewHistoryEntryRow>(id);
  if (row.profileId) {
    throw std::logic_error("Brew history entry is already complete");
  }
  row.profileId = saveProfile(entry.profile);
  row.startTime =
    std::chrono::duration_cast<std::chrono::milliseconds>(entry.brewStartTime.time_since_epoch()).count();
  row.samplingInterval =
    std::chrono::duration_cast<std::chrono::milliseconds>(entry.samplingInterval).count();
  m_storage.update(row);
  saveSamples(id, entry);

  transaction.commit();
}

openpressod::CompletedBrewHistoryEntry BrewHistoryRepository::get(DatabaseId id)
{
  auto transaction = m_storage.deferred_transaction_guard();

  const auto row = m_storage.get<BrewHistoryEntryRow>(id);
  if (!row.profileId) {
    throw std::system_error{sqlite_orm::orm_error_code::not_found};
  }
  auto result = loadEntry(row);

  transaction.commit();
  return result;
}

void BrewHistoryRepository::erase(DatabaseId id)
{
  using namespace sqlite_orm;

  auto transaction = m_storage.immediate_transaction_guard();
  const auto row = m_storage.get<BrewHistoryEntryRow>(id);
  if (!row.profileId.has_value()) {
    throw std::system_error{orm_error_code::not_found};
  }
  m_storage.remove<BrewHistoryEntryRow>(id); // ON DELETE CASCADE removes samples.
  if (m_storage.count<BrewHistoryEntryRow>(where(c(&BrewHistoryEntryRow::profileId) == row.profileId)) == 0) {
    m_storage.remove<HistoricalProfileRow>(*row.profileId); // ON DELETE CASCADE removes steps.
  }
  transaction.commit();
}

void BrewHistoryRepository::discardIncomplete()
{
  using namespace sqlite_orm;
  m_storage.remove_all<BrewHistoryEntryRow>(where(is_null(&BrewHistoryEntryRow::profileId)));
}

void BrewHistoryRepository::discardIncomplete(DatabaseId id)
{
  using namespace sqlite_orm;
  m_storage.remove_all<BrewHistoryEntryRow>(where(c(&BrewHistoryEntryRow::id) == id &&
                                                  is_null(&BrewHistoryEntryRow::profileId)));
  if (m_storage.changes() == 0) {
    throw std::system_error{orm_error_code::not_found};
  }
}

openpressod::BrewHistorySearchResult BrewHistoryRepository::find(const FindBrewHistory& query)
{
  using namespace sqlite_orm;
  using google::protobuf::util::TimeUtil;

  const int64_t limit = query.limit() == 0 ? -1LL : static_cast<int64_t>(query.limit());
  const std::optional<int64_t> minDate =
    query.has_mindate() ? std::optional{TimeUtil::TimestampToMilliseconds(query.mindate())} : std::nullopt;
  const std::optional<int64_t> maxDate =
    query.has_maxdate() ? std::optional{TimeUtil::TimestampToMilliseconds(query.maxdate())} : std::nullopt;
  const std::string pattern = query.has_profilenamepattern() ? query.profilenamepattern() : "%";
  const std::optional<uint64_t> fingerprint =
    query.has_exactprofile() ? std::optional{getFingerprint(query.exactprofile())} : std::nullopt;

  auto transaction = m_storage.deferred_transaction_guard();
  const auto rows = m_storage.get_all<BrewHistoryEntryRow>(
    where(c(&BrewHistoryEntryRow::startTime) >= ifnull<int64_t>(minDate, &BrewHistoryEntryRow::startTime) &&
          c(&BrewHistoryEntryRow::startTime) <= ifnull<int64_t>(maxDate, &BrewHistoryEntryRow::startTime) &&
          in(&BrewHistoryEntryRow::profileId,
             select(&HistoricalProfileRow::id,
                    where(like(&HistoricalProfileRow::name, pattern) &&
                          c(&HistoricalProfileRow::fingerprint) ==
                            ifnull<uint64_t>(fingerprint, &HistoricalProfileRow::fingerprint))))),
    order_by(&BrewHistoryEntryRow::id),
    sqlite_orm::limit(limit, sqlite_orm::offset(query.offset())));

  BrewHistorySearchResult result;
  for (const auto& row : rows) {
    auto entry = loadEntry(row);
    auto* resultEntry = result.add_entries();
    resultEntry->set_id(row.id);
    protobuf_helpers::toProtoTimestamp(entry.brewStartTime, *resultEntry->mutable_startpoint());
    protobuf_helpers::toProtoDuration(entry.samplingInterval, *resultEntry->mutable_samplinginterval());
    *resultEntry->mutable_profile() = std::move(entry.profile);
    for (const auto& sample : entry.samples) {
      auto* data = resultEntry->add_samples();
      data->set_pressure(sample.pressure);
      data->set_weight(sample.weight);
      data->set_flowrate(sample.flowRate);
      data->set_stepindex(static_cast<uint32_t>(sample.step));
    }
  }

  transaction.commit();
  return result;
}

openpressod::DatabaseId BrewHistoryRepository::saveProfile(const BrewProfile& profile)
{
  using namespace sqlite_orm;

  const auto fingerprint = getFingerprint(profile);
  const auto rows =
    m_storage.get_all<HistoricalProfileRow>(where(c(&HistoricalProfileRow::fingerprint) == fingerprint));
  if (!rows.empty()) {
    return rows.front().id;
  }

  HistoricalProfileRow row;
  row.name = profile.name();
  row.temperature = profile.temperature();
  row.fingerprint = fingerprint;
  if (profile.has_stopcondition()) {
    row.stopType = profile.stopcondition().type();
    row.stopValue = profile.stopcondition().value();
  }
  m_storage.insert(row);
  row.id = static_cast<DatabaseId>(m_storage.last_insert_rowid());
  saveSteps(row.id, profile);
  return row.id;
}

openpressod::BrewProfile BrewHistoryRepository::loadProfile(DatabaseId id)
{
  const auto row = m_storage.get<HistoricalProfileRow>(id);
  BrewProfile profile;
  profile.set_name(row.name);
  profile.set_temperature(row.temperature);
  if (row.stopType) {
    auto* condition = profile.mutable_stopcondition();
    condition->set_type(row.stopType.value());
    condition->set_value(row.stopValue.value());
  }
  loadSteps(id, profile);
  return profile;
}

void BrewHistoryRepository::saveSteps(DatabaseId id, const BrewProfile& profile)
{
  for (auto&& [position, step] : profile.steps() | std::views::enumerate) {
    StepRow row;
    row.profileId = id;
    row.position = position;
    row.targetType = step.targettype();
    row.targetValue = step.targetvalue();
    if (step.has_advancecondition()) {
      row.advanceType = step.advancecondition().type();
      row.advanceValue = step.advancecondition().value();
    }
    // REPLACE binds explicit composite PK columns.
    m_storage.replace(row);
  }
}

void BrewHistoryRepository::loadSteps(DatabaseId id, BrewProfile& profile)
{
  using namespace sqlite_orm;

  for (const auto& row :
       m_storage.get_all<StepRow>(where(c(&StepRow::profileId) == id), order_by(&StepRow::position))) {
    auto* step = profile.add_steps();
    step->set_targettype(row.targetType);
    step->set_targetvalue(row.targetValue);
    if (row.advanceType) {
      auto* condition = step->mutable_advancecondition();
      condition->set_type(row.advanceType.value());
      condition->set_value(row.advanceValue.value());
    }
  }
}

void BrewHistoryRepository::saveSamples(DatabaseId id, const CompletedBrewHistoryEntry& entry)
{
  for (auto&& [position, sample] : entry.samples | std::views::enumerate) {
    BrewHistoryEntrySampleRow row;
    row.brewId = id;
    row.position = position;
    row.pressure = sample.pressure;
    row.weight = sample.weight;
    row.flowRate = sample.flowRate;
    row.step = sample.step;
    m_storage.replace(row);
  }
}

openpressod::CompletedBrewHistoryEntry BrewHistoryRepository::loadEntry(const BrewHistoryEntryRow& row)
{
  using namespace sqlite_orm;

  CompletedBrewHistoryEntry entry{};
  entry.brewStartTime = std::chrono::system_clock::time_point{
    std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::milliseconds{row.startTime})};
  entry.samplingInterval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
    std::chrono::milliseconds{row.samplingInterval});
  entry.profile = loadProfile(row.profileId.value());
  for (const auto& sample : m_storage.get_all<BrewHistoryEntrySampleRow>(
         where(c(&BrewHistoryEntrySampleRow::brewId) == row.id),
         order_by(&BrewHistoryEntrySampleRow::position))) {
    entry.samples.push_back({
      .pressure = sample.pressure,
      .weight = sample.weight,
      .flowRate = sample.flowRate,
      .step = sample.step,
    });
  }
  return entry;
}

#include "repository.hpp"

#include "brew_log/brew_log_data.hpp"
#include "database/brew_profile_fingerprint.hpp"
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
#include <openpresso_proto/brew_log.pb.h>
#include <openpresso_proto/brew_profile.pb.h>
#include <sqlite_orm/sqlite_orm.h>

using namespace openpressod::db::brew_history;

BrewHistoryRepository::BrewHistoryRepository(std::string path)
: m_storage(makeStorage(std::move(path)))
{
  m_storage.open_forever();
  m_storage.sync_schema();
}

uint64_t BrewHistoryRepository::createEmptyLog()
{
  auto transaction = m_storage.immediate_transaction_guard();

  BrewLogRow row;
  m_storage.insert(row);
  // sqlite_orm::insert returns int; retain SQLite's full 64-bit row ID.
  row.id = static_cast<uint64_t>(m_storage.last_insert_rowid());

  transaction.commit();
  return row.id;
}

void BrewHistoryRepository::completeLog(uint64_t id, const CompletedBrewLog& log, const BrewProfile& profile)
{
  auto transaction = m_storage.immediate_transaction_guard();

  auto row = m_storage.get<BrewLogRow>(id);
  if (row.profileId) {
    throw std::logic_error("Brew log is already complete");
  }
  row.profileId = saveProfile(profile);
  row.startTime =
    std::chrono::duration_cast<std::chrono::milliseconds>(log.brewStartTime.time_since_epoch()).count();
  row.samplingInterval =
    std::chrono::duration_cast<std::chrono::milliseconds>(log.samplingInterval).count();
  m_storage.update(row);
  saveRecords(id, log);

  transaction.commit();
}

std::pair<openpressod::CompletedBrewLog, openpressod::BrewProfile> BrewHistoryRepository::get(uint64_t id)
{
  auto transaction = m_storage.deferred_transaction_guard();

  const auto row = m_storage.get<BrewLogRow>(id);
  if (!row.profileId) {
    throw std::system_error{sqlite_orm::orm_error_code::not_found};
  }
  auto result = std::pair{loadLog(row), loadProfile(*row.profileId)};

  transaction.commit();
  return result;
}

void BrewHistoryRepository::erase(uint64_t id)
{
  using namespace sqlite_orm;

  auto transaction = m_storage.immediate_transaction_guard();
  const auto row = m_storage.get<BrewLogRow>(id);
  if (!row.profileId.has_value()) {
    throw std::system_error{orm_error_code::not_found};
  }
  m_storage.remove<BrewLogRow>(id); // ON DELETE CASCADE removes records.
  if (m_storage.count<BrewLogRow>(where(c(&BrewLogRow::profileId) == row.profileId)) == 0) {
    m_storage.remove<HistoricalProfileRow>(*row.profileId); // ON DELETE CASCADE removes steps.
  }
  transaction.commit();
}

void BrewHistoryRepository::discardIncomplete()
{
  using namespace sqlite_orm;
  m_storage.remove_all<BrewLogRow>(where(is_null(&BrewLogRow::profileId)));
}

void BrewHistoryRepository::discardIncomplete(uint64_t id)
{
  using namespace sqlite_orm;
  m_storage.remove_all<BrewLogRow>(where(c(&BrewLogRow::id) == id && is_null(&BrewLogRow::profileId)));
  if (m_storage.changes() == 0) {
    throw std::system_error{orm_error_code::not_found};
  }
}

openpressod::BrewLogsSearchResult BrewHistoryRepository::find(const FindBrewLogs& query)
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
  const auto rows = m_storage.get_all<BrewLogRow>(
    where(c(&BrewLogRow::startTime) >= ifnull<int64_t>(minDate, &BrewLogRow::startTime) &&
          c(&BrewLogRow::startTime) <= ifnull<int64_t>(maxDate, &BrewLogRow::startTime) &&
          in(&BrewLogRow::profileId,
             select(&HistoricalProfileRow::id,
                    where(like(&HistoricalProfileRow::name, pattern) &&
                          c(&HistoricalProfileRow::fingerprint) ==
                            ifnull<uint64_t>(fingerprint, &HistoricalProfileRow::fingerprint))))),
    order_by(&BrewLogRow::id),
    sqlite_orm::limit(limit, sqlite_orm::offset(query.offset())));

  BrewLogsSearchResult result;
  for (const auto& row : rows) {
    const auto log = loadLog(row);
    auto* brewLog = result.add_logs();
    brewLog->set_id(row.id);
    protobuf_helpers::toProtoTimestamp(log.brewStartTime, *brewLog->mutable_startpoint());
    protobuf_helpers::toProtoDuration(log.samplingInterval, *brewLog->mutable_samplinginterval());
    *brewLog->mutable_profile() = loadProfile(row.profileId.value());
    for (const auto& record : log.records) {
      auto* data = brewLog->add_records();
      data->set_pressure(record.pressure);
      data->set_weight(record.weight);
      data->set_flowrate(record.flowRate);
      data->set_stepindex(static_cast<uint32_t>(record.step));
    }
  }

  transaction.commit();
  return result;
}

uint64_t BrewHistoryRepository::saveProfile(const BrewProfile& profile)
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
  row.id = static_cast<uint64_t>(m_storage.last_insert_rowid());
  saveSteps(row.id, profile);
  return row.id;
}

openpressod::BrewProfile BrewHistoryRepository::loadProfile(uint64_t id)
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

void BrewHistoryRepository::saveSteps(uint64_t id, const BrewProfile& profile)
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

void BrewHistoryRepository::loadSteps(uint64_t id, BrewProfile& profile)
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

void BrewHistoryRepository::saveRecords(uint64_t id, const CompletedBrewLog& log)
{
  for (auto&& [position, record] : log.records | std::views::enumerate) {
    BrewLogRecordRow row;
    row.brewId = id;
    row.position = position;
    row.pressure = record.pressure;
    row.weight = record.weight;
    row.flowRate = record.flowRate;
    row.step = record.step;
    m_storage.replace(row);
  }
}

openpressod::CompletedBrewLog BrewHistoryRepository::loadLog(const BrewLogRow& row)
{
  using namespace sqlite_orm;

  CompletedBrewLog log{};
  log.brewStartTime = std::chrono::system_clock::time_point{
    std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::milliseconds{row.startTime})};
  log.samplingInterval = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
    std::chrono::milliseconds{row.samplingInterval});
  for (const auto& record : m_storage.get_all<BrewLogRecordRow>(
         where(c(&BrewLogRecordRow::brewId) == row.id), order_by(&BrewLogRecordRow::position))) {
    log.records.push_back({
      .pressure = record.pressure,
      .weight = record.weight,
      .flowRate = record.flowRate,
      .step = record.step,
    });
  }
  return log;
}

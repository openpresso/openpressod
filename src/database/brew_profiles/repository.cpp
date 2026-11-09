#include "repository.hpp"

#include "database/brew_profile_fingerprint.hpp"
#include "database/database_id.hpp"
#include "database/steps_schema.hpp"
#include "schema.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <ranges>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <utility>

#include <google/protobuf/util/time_util.h>
#include <openpresso_proto/brew_profile.pb.h>
#include <sqlite_orm/sqlite_orm.h>

using namespace openpressod::db::brew_profiles;

BrewProfilesRepository::BrewProfilesRepository(std::string path)
: m_storage(makeStorage(std::move(path)))
{
  m_storage.on_open = [](sqlite3* connection) {
    const auto result = sqlite3_extended_result_codes(connection, 1);
    if (result != SQLITE_OK) {
      throw std::system_error{sqlite_orm::sqlite_errc(result)};
    }
  };
  m_storage.open_forever();
  m_storage.sync_schema();
}

openpressod::DatabaseId BrewProfilesRepository::create(const BrewProfile& profile)
{
  auto transaction = m_storage.immediate_transaction_guard();

  auto now = nowSeconds();
  ProfileRow row = makeProfileRow(0, profile);
  row.created = now;
  row.modified = now;
  m_storage.insert(row);
  // sqlite_orm::insert returns int; retain SQLite's full 64-bit row ID.
  row.id = static_cast<DatabaseId>(m_storage.last_insert_rowid());
  saveSteps(row.id, profile);
  transaction.commit();
  return row.id;
}

void BrewProfilesRepository::update(DatabaseId id, const BrewProfile& profile)
{
  auto transaction = m_storage.immediate_transaction_guard();

  auto previous = m_storage.get<ProfileRow>(id);
  ProfileRow row = makeProfileRow(id, profile);
  row.created = previous.created;
  row.modified = nowSeconds();
  m_storage.update(row);

  m_storage.remove_all<StepRow>(sqlite_orm::where(sqlite_orm::c(&StepRow::profileId) == id));
  saveSteps(id, profile);
  transaction.commit();
}

openpressod::PersistentBrewProfile BrewProfilesRepository::get(DatabaseId id)
{
  auto transaction = m_storage.deferred_transaction_guard();

  auto row = m_storage.get<ProfileRow>(id);

  PersistentBrewProfile result;
  toPersistentProfile(row, result);
  loadSteps(id, *result.mutable_profile());

  transaction.commit();

  return result;
}

void BrewProfilesRepository::erase(DatabaseId id)
{
  m_storage.remove<ProfileRow>(id); // ON DELETE CASCADE removes steps
  if (m_storage.changes() == 0) {
    throw std::system_error{sqlite_orm::orm_error_code::not_found};
  }
}

openpressod::PersistentBrewProfile BrewProfilesRepository::find(const BrewProfile& profile)
{
  using namespace sqlite_orm;
  auto fingerprint = getFingerprint(profile);

  auto transaction = m_storage.deferred_transaction_guard();
  auto rows = m_storage.get_all<ProfileRow>(where(c(&ProfileRow::fingerprint) == fingerprint));

  if (rows.empty()) {
    throw std::system_error{orm_error_code::not_found};
  }

  PersistentBrewProfile result;
  toPersistentProfile(rows.front(), result);
  loadSteps(rows.front().id, *result.mutable_profile());
  return result;
}

openpressod::BrewProfilesSearchResult BrewProfilesRepository::find(const FindBrewProfiles& query)
{
  using namespace sqlite_orm;
  using google::protobuf::util::TimeUtil;

  const int64_t limit = query.limit() == 0 ? -1LL : static_cast<int64_t>(query.limit());

  auto timestamp = [](const auto& value) -> uint64_t { return TimeUtil::TimestampToSeconds(value); };

  const std::optional<int64_t> minCreated =
    query.has_mincreated() ? std::optional{timestamp(query.mincreated())} : std::nullopt;

  const std::optional<int64_t> maxCreated =
    query.has_maxcreated() ? std::optional{timestamp(query.maxcreated())} : std::nullopt;

  const std::optional<int64_t> minModified =
    query.has_minmodified() ? std::optional{timestamp(query.minmodified())} : std::nullopt;

  const std::optional<int64_t> maxModified =
    query.has_maxmodified() ? std::optional{timestamp(query.maxmodified())} : std::nullopt;

  const std::string pattern = query.has_namepattern() ? query.namepattern() : "%";

  auto transaction = m_storage.deferred_transaction_guard();

  const auto rows = m_storage.get_all<ProfileRow>(
    where(c(&ProfileRow::created) >= ifnull<uint64_t>(minCreated, &ProfileRow::created) &&
          c(&ProfileRow::created) <= ifnull<uint64_t>(maxCreated, &ProfileRow::created) &&
          c(&ProfileRow::modified) >= ifnull<uint64_t>(minModified, &ProfileRow::modified) &&
          c(&ProfileRow::modified) <= ifnull<uint64_t>(maxModified, &ProfileRow::modified) &&
          like(&ProfileRow::name, pattern)),
    order_by(&ProfileRow::id),
    sqlite_orm::limit(limit, sqlite_orm::offset(query.offset())));

  BrewProfilesSearchResult result;
  for (const auto& row : rows) {
    auto* profile = result.add_profiles();
    toPersistentProfile(row, *profile);
    loadSteps(row.id, *profile->mutable_profile());
  }

  transaction.commit();

  return result;
}

ProfileRow BrewProfilesRepository::makeProfileRow(DatabaseId id, const BrewProfile& profile)
{
  ProfileRow row;
  row.id = id;
  row.name = profile.name();
  row.temperature = profile.temperature();
  if (profile.has_stopcondition()) {
    row.stopType = profile.stopcondition().type();
    row.stopValue = profile.stopcondition().value();
  }
  row.fingerprint = getFingerprint(profile);

  return row;
}

uint64_t BrewProfilesRepository::nowSeconds()
{
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

void BrewProfilesRepository::saveSteps(DatabaseId id, const BrewProfile& profile)
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
    // REPLACE binds explicit composite PK columns
    m_storage.replace(row);
  }
}

void BrewProfilesRepository::loadSteps(DatabaseId id, BrewProfile& profile)
{
  for (const auto& step :
       m_storage.get_all<StepRow>(sqlite_orm::where(sqlite_orm::c(&StepRow::profileId) == id),
                                  sqlite_orm::order_by(&StepRow::position))) {
    auto* profileStep = profile.add_steps();
    profileStep->set_targettype(static_cast<openpressod::BrewStepTargetType>(step.targetType));
    profileStep->set_targetvalue(step.targetValue);
    if (step.advanceType) {
      auto* condition = profileStep->mutable_advancecondition();
      condition->set_type(step.advanceType.value());
      condition->set_value(step.advanceValue.value());
    }
  }
}

void BrewProfilesRepository::toPersistentProfile(const ProfileRow& row, PersistentBrewProfile& profile)
{
  using google::protobuf::util::TimeUtil;
  profile.set_id(row.id);
  profile.mutable_profile()->set_name(row.name);
  profile.mutable_profile()->set_temperature(row.temperature);
  if (row.stopType.has_value()) {
    profile.mutable_profile()->mutable_stopcondition()->set_type(row.stopType.value());
    profile.mutable_profile()->mutable_stopcondition()->set_value(row.stopValue.value());
  }
  *profile.mutable_created() = TimeUtil::SecondsToTimestamp(static_cast<int64_t>(row.created));
  *profile.mutable_modified() = TimeUtil::SecondsToTimestamp(static_cast<int64_t>(row.modified));
}

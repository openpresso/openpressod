#include "database/brew_history/repository.hpp"
#include "database/database_id.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>
#include <gtest/gtest.h>

using namespace openpressod;
using namespace openpressod::db::brew_history;
using openpressod::db::StepRow;
using namespace std::chrono_literals;

namespace
{

BrewProfile makeProfile(std::string name = "Espresso")
{
  BrewProfile profile;
  profile.set_name(std::move(name));
  profile.set_temperature(93'000);
  profile.mutable_stopcondition()->set_type(StopConditionType_TotalWeight);
  profile.mutable_stopcondition()->set_value(36.5F);
  for (int i = 0; i < 3; ++i) {
    auto* step = profile.add_steps();
    step->set_targettype(i == 1 ? BrewStepTargetType_FlowRate : BrewStepTargetType_Pressure);
    step->set_targetvalue(2.25F + i);
    if (i != 1) {
      step->mutable_advancecondition()->set_type(BrewStepAdvanceConditionType_StepTime);
      step->mutable_advancecondition()->set_value(5.5F + i);
    }
  }
  return profile;
}

CompletedBrewHistoryEntry makeEntry(std::chrono::nanoseconds start = 100s + 123ms,
                                    BrewProfile profile = makeProfile())
{
  return {
    .brewStartTime = std::chrono::system_clock::time_point{start},
    .samplingInterval = 10ms,
    .profile = std::move(profile),
    .samples = {{8'500, -200, -300, 2}, {9'000, 36'000, 2'100, 0}, {7'000, 40'000, 100, 1}},
  };
}

std::vector<DatabaseId> ids(const BrewHistorySearchResult& result)
{
  std::vector<DatabaseId> resultIds;
  for (const auto& entry : result.entries()) {
    resultIds.push_back(entry.id());
  }
  return resultIds;
}

class BrewHistoryRepositoryTest : public testing::Test {
protected:
  void SetUp() override
  {
    auto base = std::filesystem::temp_directory_path() /
                ("openpressod-history-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    directory = base;
    for (unsigned suffix = 0; !std::filesystem::create_directory(directory); ++suffix) {
      directory = base.string() + "-" + std::to_string(suffix);
    }
    path = (directory / "history.sqlite").string();
    repository = std::make_unique<BrewHistoryRepository>(path);
  }

  void TearDown() override
  {
    repository.reset();
    std::error_code error;
    std::filesystem::remove_all(directory, error);
  }

  void sql(const std::string& statement)
  {
    sqlite3* connection = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(path.c_str(), &connection));
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> guard(connection, sqlite3_close);
    const auto result = sqlite3_exec(connection, statement.c_str(), nullptr, nullptr, nullptr);
    ASSERT_EQ(SQLITE_OK, result) << sqlite3_errmsg(connection);
  }

  DatabaseId createEntry(const CompletedBrewHistoryEntry& entry)
  {
    const auto id = repository->createEmptyEntry();
    repository->completeEntry(id, entry);
    return id;
  }

  void expectEntry(const CompletedBrewHistoryEntry& expected, const CompletedBrewHistoryEntry& actual)
  {
    EXPECT_EQ(expected.profile.SerializeAsString(), actual.profile.SerializeAsString());
    EXPECT_EQ(expected.brewStartTime, actual.brewStartTime);
    EXPECT_EQ(expected.samplingInterval, actual.samplingInterval);
    ASSERT_EQ(expected.samples.size(), actual.samples.size());
    for (size_t i = 0; i < expected.samples.size(); ++i) {
      EXPECT_EQ(expected.samples[i].pressure, actual.samples[i].pressure);
      EXPECT_EQ(expected.samples[i].weight, actual.samples[i].weight);
      EXPECT_EQ(expected.samples[i].flowRate, actual.samples[i].flowRate);
      EXPECT_EQ(expected.samples[i].step, actual.samples[i].step);
    }
  }

  void expectCounts(int entries, int profiles, int samples, int steps)
  {
    auto storage = makeStorage(path);
    EXPECT_EQ(entries, storage.count<BrewHistoryEntryRow>());
    EXPECT_EQ(profiles, storage.count<HistoricalProfileRow>());
    EXPECT_EQ(samples, storage.count<BrewHistoryEntrySampleRow>());
    EXPECT_EQ(steps, storage.count<StepRow>());
  }

  std::filesystem::path directory;
  std::string path;
  std::unique_ptr<BrewHistoryRepository> repository;
};

TEST_F(BrewHistoryRepositoryTest, FreshDatabaseUsesHistoryEntryAndSampleSchema)
{
  sqlite3* connection = nullptr;
  ASSERT_EQ(SQLITE_OK, sqlite3_open(path.c_str(), &connection));
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> guard(connection, sqlite3_close);
  std::vector<std::string> names;
  const auto collect = [](void* context, int, char** values, char**) {
    static_cast<std::vector<std::string>*>(context)->emplace_back(values[0]);
    return 0;
  };
  ASSERT_EQ(
    SQLITE_OK,
    sqlite3_exec(
      connection,
      "SELECT name FROM sqlite_master WHERE name LIKE '%brew_%' AND sql IS NOT NULL ORDER BY name",
      collect,
      &names,
      nullptr));
  EXPECT_EQ((std::vector<std::string>{"brew_history_entries",
                                      "brew_history_entry_samples",
                                      "ix_brew_history_entries_profile",
                                      "ix_brew_history_entries_start"}),
            names);
}

TEST_F(BrewHistoryRepositoryTest, EmptyDatabaseAndMissingIds)
{
  EXPECT_TRUE(repository->find(FindBrewHistory{}).entries().empty());
  EXPECT_THROW(repository->get(42), std::system_error);
  EXPECT_THROW(repository->completeEntry(42, makeEntry()), std::system_error);
  EXPECT_THROW(repository->erase(42), std::system_error);
  EXPECT_THROW(repository->discardIncomplete(42), std::system_error);
  EXPECT_NO_THROW(repository->discardIncomplete());
  expectCounts(0, 0, 0, 0);
}

TEST_F(BrewHistoryRepositoryTest, EmptyEntriesReservePersistentIdsWithoutCreatingProfiles)
{
  const auto first = repository->createEmptyEntry();
  const auto second = repository->createEmptyEntry();
  EXPECT_GT(first, 0U);
  EXPECT_GT(second, first);
  expectCounts(2, 0, 0, 0);
  EXPECT_THROW(repository->get(first), std::system_error);
  EXPECT_TRUE(repository->find(FindBrewHistory{}).entries().empty());
  repository.reset();
  repository = std::make_unique<BrewHistoryRepository>(path);
  EXPECT_THROW(repository->get(first), std::system_error);
  repository->completeEntry(first, makeEntry());
  expectEntry(makeEntry(), repository->get(first));
  expectCounts(2, 1, 3, 3);
  repository->discardIncomplete(second);
  expectCounts(1, 1, 3, 3);
  EXPECT_THROW(repository->completeEntry(second, makeEntry()), std::system_error);
  repository->erase(first);
  const auto empty = repository->createEmptyEntry();
  repository->discardIncomplete(empty);
  expectCounts(0, 0, 0, 0);
}

TEST_F(BrewHistoryRepositoryTest, ErasePreservesIncompleteEntriesAndTheyCanStillBeCompleted)
{
  const auto incomplete = repository->createEmptyEntry();
  const auto complete = createEntry(makeEntry());

  EXPECT_THROW(repository->erase(incomplete), std::system_error);
  expectCounts(2, 1, 3, 3);
  EXPECT_THROW(repository->get(incomplete), std::system_error);
  EXPECT_EQ((std::vector<DatabaseId>{complete}), ids(repository->find(FindBrewHistory{})));

  repository->completeEntry(incomplete, makeEntry());
  expectEntry(makeEntry(), repository->get(incomplete));
  repository->erase(incomplete);
  expectCounts(1, 1, 3, 3);
}

TEST_F(BrewHistoryRepositoryTest, DiscardIncompletePreservesCompletedEntriesAndTheirData)
{
  const auto incomplete = repository->createEmptyEntry();
  const auto otherIncomplete = repository->createEmptyEntry();
  const auto complete = createEntry(makeEntry());
  const auto original = repository->find(FindBrewHistory{});

  EXPECT_THROW(repository->discardIncomplete(complete), std::system_error);
  expectCounts(3, 1, 3, 3);
  EXPECT_EQ(original.SerializeAsString(), repository->find(FindBrewHistory{}).SerializeAsString());

  repository->discardIncomplete(incomplete);
  expectCounts(2, 1, 3, 3);
  EXPECT_THROW(repository->completeEntry(incomplete, makeEntry()), std::system_error);
  EXPECT_THROW(repository->discardIncomplete(incomplete), std::system_error);
  repository->completeEntry(otherIncomplete, makeEntry());
  expectCounts(2, 1, 6, 3);
}

TEST_F(BrewHistoryRepositoryTest, DiscardAllIncompletePreservesCompletedEntriesAndTheirData)
{
  const auto firstIncomplete = repository->createEmptyEntry();
  createEntry(makeEntry());
  const auto secondIncomplete = repository->createEmptyEntry();
  createEntry(makeEntry(200s));
  auto emptyEntry = makeEntry(300s, makeProfile("Other"));
  emptyEntry.samples.clear();
  createEntry(emptyEntry);
  const auto original = repository->find(FindBrewHistory{});
  expectCounts(5, 2, 6, 6);

  repository->discardIncomplete();
  expectCounts(3, 2, 6, 6);
  EXPECT_EQ(original.SerializeAsString(), repository->find(FindBrewHistory{}).SerializeAsString());
  EXPECT_THROW(repository->completeEntry(firstIncomplete, makeEntry()), std::system_error);
  EXPECT_THROW(repository->completeEntry(secondIncomplete, makeEntry()), std::system_error);

  EXPECT_NO_THROW(repository->discardIncomplete());
  expectCounts(3, 2, 6, 6);
  EXPECT_EQ(original.SerializeAsString(), repository->find(FindBrewHistory{}).SerializeAsString());
}

TEST_F(BrewHistoryRepositoryTest, DiscardAllIncompleteClearsReservationsAndPreservesIdSequence)
{
  const auto first = repository->createEmptyEntry();
  const auto second = repository->createEmptyEntry();

  repository->discardIncomplete();
  expectCounts(0, 0, 0, 0);
  EXPECT_THROW(repository->completeEntry(first, makeEntry()), std::system_error);
  EXPECT_THROW(repository->completeEntry(second, makeEntry()), std::system_error);

  const auto next = repository->createEmptyEntry();
  EXPECT_GT(next, second);
  repository->completeEntry(next, makeEntry());
  expectEntry(makeEntry(), repository->get(next));
  expectCounts(1, 1, 3, 3);
}

TEST_F(BrewHistoryRepositoryTest, CompletingAnAlreadyCompletedEntryLeavesHistoryUnchanged)
{
  const auto id = createEntry(makeEntry());
  const auto original = repository->find(FindBrewHistory{});
  EXPECT_THROW(repository->completeEntry(id, makeEntry(200s, makeProfile("Replacement"))), std::logic_error);
  EXPECT_EQ(original.SerializeAsString(), repository->find(FindBrewHistory{}).SerializeAsString());
  expectCounts(1, 1, 3, 3);
  EXPECT_NO_THROW(createEntry(makeEntry()));
  expectCounts(2, 1, 6, 3);
}

TEST_F(BrewHistoryRepositoryTest, RoundTripsAllFieldsAndFindReturnsCompleteEntries)
{
  const auto profile = makeProfile("Café 'special' ☕");
  const auto entry = makeEntry(100s + 123ms, profile);
  const auto id = createEntry(entry);
  const auto storedEntry = repository->get(id);
  EXPECT_GT(id, 0U);
  expectEntry(entry, storedEntry);

  const auto result = repository->find(FindBrewHistory{});
  ASSERT_EQ(1, result.entries_size());
  const auto& found = result.entries(0);
  EXPECT_EQ(id, found.id());
  EXPECT_EQ(profile.SerializeAsString(), found.profile().SerializeAsString());
  EXPECT_EQ(100, found.startpoint().seconds());
  EXPECT_EQ(123'000'000, found.startpoint().nanos());
  EXPECT_EQ(0, found.samplinginterval().seconds());
  EXPECT_EQ(10'000'000, found.samplinginterval().nanos());
  ASSERT_EQ(entry.samples.size(), found.samples_size());
  for (size_t i = 0; i < entry.samples.size(); ++i) {
    const auto& sample = found.samples(static_cast<int>(i));
    EXPECT_EQ(entry.samples[i].pressure, sample.pressure());
    EXPECT_EQ(entry.samples[i].weight, sample.weight());
    EXPECT_EQ(entry.samples[i].flowRate, sample.flowrate());
    EXPECT_EQ(entry.samples[i].step, sample.stepindex());
  }
}

TEST_F(BrewHistoryRepositoryTest, StoresTimesInMillisecondsAndTruncatesSubmillisecondPrecision)
{
  auto entry = makeEntry(100s + 123ms + 456us);
  entry.samplingInterval = 10ms + 321us;
  const auto id = createEntry(entry);
  auto storage = makeStorage(path);
  const auto row = storage.get<BrewHistoryEntryRow>(id);
  EXPECT_EQ(100'123, row.startTime);
  EXPECT_EQ(10, row.samplingInterval);
  expectEntry(makeEntry(), repository->get(id));
  const auto result = repository->find(FindBrewHistory{});
  ASSERT_EQ(1, result.entries_size());
  EXPECT_EQ(123'000'000, result.entries(0).startpoint().nanos());
  EXPECT_EQ(10'000'000, result.entries(0).samplinginterval().nanos());
}

TEST_F(BrewHistoryRepositoryTest, EmptyEntriesAndAbsentAndDefaultConditionsRemainDistinct)
{
  auto entry = makeEntry();
  entry.profile.Clear();
  entry.profile.add_steps();
  entry.samples.clear();
  const auto absent = createEntry(entry);
  EXPECT_EQ(entry.profile.SerializeAsString(), repository->get(absent).profile.SerializeAsString());
  EXPECT_TRUE(repository->get(absent).samples.empty());
  entry.profile.mutable_stopcondition();
  const auto stop = createEntry(entry);
  EXPECT_EQ(entry.profile.SerializeAsString(), repository->get(stop).profile.SerializeAsString());
  entry.profile.mutable_steps(0)->mutable_advancecondition();
  const auto advance = createEntry(entry);
  EXPECT_EQ(entry.profile.SerializeAsString(), repository->get(advance).profile.SerializeAsString());
  entry.profile.clear_steps();
  const auto noSteps = createEntry(entry);
  EXPECT_EQ(entry.profile.SerializeAsString(), repository->get(noSteps).profile.SerializeAsString());
  expectCounts(4, 4, 0, 3);
}

TEST_F(BrewHistoryRepositoryTest, ReusesProfilesUntilLastReferenceIsDeleted)
{
  const auto profile = makeProfile();
  const auto first = createEntry(makeEntry(100s + 123ms, profile));
  const auto second = createEntry(makeEntry(200s, profile));
  const auto other = createEntry(makeEntry(100s + 123ms, makeProfile("Other")));
  expectCounts(3, 2, 9, 6);
  auto storage = makeStorage(path);
  const auto profileId = storage.get<BrewHistoryEntryRow>(first).profileId;
  EXPECT_EQ(profileId, storage.get<BrewHistoryEntryRow>(second).profileId);
  repository->erase(first);
  expectCounts(2, 2, 6, 6);
  EXPECT_THROW(repository->get(first), std::system_error);
  EXPECT_EQ(profile.SerializeAsString(), repository->get(second).profile.SerializeAsString());
  repository->erase(second);
  expectCounts(1, 1, 3, 3);
  EXPECT_FALSE(storage.get_optional<HistoricalProfileRow>(profileId.value()));
  EXPECT_EQ("Other", repository->get(other).profile.name());
  EXPECT_THROW(repository->erase(second), std::system_error);
  const auto recreated = createEntry(makeEntry(100s + 123ms, profile));
  EXPECT_NE(profileId, storage.get<BrewHistoryEntryRow>(recreated).profileId);
  expectCounts(2, 2, 6, 6);
}

TEST_F(BrewHistoryRepositoryTest, FingerprintDistinguishesEveryFieldAndStepOrder)
{
  const auto original = makeProfile();
  createEntry(makeEntry(100s + 123ms, original));
  int profiles = 1;
  auto distinct = [&](const char* field, auto mutate) {
    SCOPED_TRACE(field);
    auto profile = original;
    mutate(profile);
    const auto id = createEntry(makeEntry(100s + 123ms, profile));
    EXPECT_EQ(profile.SerializeAsString(), repository->get(id).profile.SerializeAsString());
    auto storage = makeStorage(path);
    EXPECT_EQ(++profiles, storage.count<HistoricalProfileRow>());
    createEntry(makeEntry(100s + 123ms, profile));
    EXPECT_EQ(profiles, storage.count<HistoricalProfileRow>());
  };
  distinct("name", [](auto& p) { p.set_name("espresso"); });
  distinct("temperature", [](auto& p) { p.set_temperature(94'000); });
  distinct("stop presence", [](auto& p) { p.clear_stopcondition(); });
  distinct("stop type",
           [](auto& p) { p.mutable_stopcondition()->set_type(StopConditionType_TotalTime); });
  distinct("stop value", [](auto& p) { p.mutable_stopcondition()->set_value(37.5F); });
  distinct("step count", [](auto& p) { p.mutable_steps()->RemoveLast(); });
  distinct("step order", [](auto& p) { p.mutable_steps()->SwapElements(0, 2); });
  distinct("target type",
           [](auto& p) { p.mutable_steps(0)->set_targettype(BrewStepTargetType_FlowRate); });
  distinct("target value", [](auto& p) { p.mutable_steps(0)->set_targetvalue(9.5F); });
  distinct("advance presence", [](auto& p) { p.mutable_steps(0)->clear_advancecondition(); });
  distinct("advance type", [](auto& p) {
    p.mutable_steps(0)->mutable_advancecondition()->set_type(BrewStepAdvanceConditionType_TotalWeight);
  });
  distinct("advance value",
           [](auto& p) { p.mutable_steps(0)->mutable_advancecondition()->set_value(10.5F); });
}

TEST_F(BrewHistoryRepositoryTest, FailedCompletionRollsBackAllNewRowsAndCanBeRetried)
{
  const auto profile = makeProfile();
  const auto id = createEntry(makeEntry(100s + 123ms, profile));
  const auto first = repository->createEmptyEntry();
  const auto second = repository->createEmptyEntry();
  sql("CREATE TRIGGER fail_step BEFORE INSERT ON steps WHEN NEW.position = 1 "
      "BEGIN SELECT RAISE(ABORT, 'injected step failure'); END");
  EXPECT_THROW(repository->completeEntry(first, makeEntry(100s + 123ms, makeProfile("Rejected"))),
               std::system_error);
  expectCounts(3, 1, 3, 3);
  sql("DROP TRIGGER fail_step");
  sql(
    "CREATE TRIGGER fail_sample BEFORE INSERT ON brew_history_entry_samples WHEN NEW.position = 1 "
    "BEGIN SELECT RAISE(ABORT, 'injected sample failure'); END");
  EXPECT_THROW(repository->completeEntry(first, makeEntry(100s + 123ms, makeProfile("Rejected"))),
               std::system_error);
  EXPECT_THROW(repository->completeEntry(second, makeEntry(100s + 123ms, profile)), std::system_error);
  expectCounts(3, 1, 3, 3);
  auto storage = makeStorage(path);
  for (const auto pending : {first, second}) {
    const auto row = storage.get<BrewHistoryEntryRow>(pending);
    EXPECT_FALSE(row.profileId.has_value());
    EXPECT_EQ(0, row.startTime);
    EXPECT_EQ(0, row.samplingInterval);
    EXPECT_THROW(repository->get(pending), std::system_error);
  }
  EXPECT_EQ((std::vector<DatabaseId>{id}), ids(repository->find(FindBrewHistory{})));
  expectEntry(makeEntry(), repository->get(id));
  sql("DROP TRIGGER fail_sample");
  EXPECT_NO_THROW(repository->completeEntry(first, makeEntry(100s + 123ms, makeProfile("Accepted"))));
  EXPECT_NO_THROW(repository->completeEntry(second, makeEntry(100s + 123ms, profile)));
  expectCounts(3, 2, 9, 6);
  expectEntry(makeEntry(100s + 123ms, makeProfile("Accepted")), repository->get(first));
  expectEntry(makeEntry(), repository->get(second));
}

TEST_F(BrewHistoryRepositoryTest, FailedProfileCleanupRestoresEntryAndSamples)
{
  const auto id = createEntry(makeEntry());
  sql("CREATE TRIGGER fail_cleanup BEFORE DELETE ON profiles "
      "BEGIN SELECT RAISE(ABORT, 'injected cleanup failure'); END");
  EXPECT_THROW(repository->erase(id), std::system_error);
  expectCounts(1, 1, 3, 3);
  expectEntry(makeEntry(), repository->get(id));
  sql("DROP TRIGGER fail_cleanup");
  repository->erase(id);
  expectCounts(0, 0, 0, 0);
}

TEST_F(BrewHistoryRepositoryTest, ReopeningPreservesEntriesAndProfileReferences)
{
  const auto first = createEntry(makeEntry());
  const auto second = createEntry(makeEntry(200s));
  repository->erase(first);
  const auto expected = repository->find(FindBrewHistory{});
  repository.reset();
  repository = std::make_unique<BrewHistoryRepository>(path);
  EXPECT_EQ(expected.SerializeAsString(), repository->find(FindBrewHistory{}).SerializeAsString());
  EXPECT_THROW(repository->get(first), std::system_error);
  expectEntry(makeEntry(200s), repository->get(second));
  createEntry(makeEntry());
  expectCounts(2, 1, 6, 3);
}

TEST_F(BrewHistoryRepositoryTest, PaginationUsesIdOrderAndZeroLimitMeansUnlimited)
{
  repository->createEmptyEntry();
  const auto first = createEntry(makeEntry(300s));
  repository->createEmptyEntry();
  const auto second = createEntry(makeEntry(100s));
  const auto third = createEntry(makeEntry(200s));
  FindBrewHistory query;
  EXPECT_EQ((std::vector<DatabaseId>{first, second, third}), ids(repository->find(query)));
  query.set_offset(1);
  EXPECT_EQ((std::vector<DatabaseId>{second, third}), ids(repository->find(query)));
  query.set_limit(1);
  EXPECT_EQ((std::vector<DatabaseId>{second}), ids(repository->find(query)));
  query.set_offset(3);
  EXPECT_TRUE(ids(repository->find(query)).empty());
}

TEST_F(BrewHistoryRepositoryTest, ProfileFiltersApplyBeforePagination)
{
  createEntry(makeEntry(100s + 123ms, makeProfile("Other")));
  const auto profile = makeProfile();
  const auto first = createEntry(makeEntry(100s + 123ms, profile));
  auto changed = profile;
  changed.set_temperature(94'000);
  const auto second = createEntry(makeEntry(100s + 123ms, changed));
  const auto third = createEntry(makeEntry(100s + 123ms, makeProfile("espresso long")));
  FindBrewHistory query;
  query.set_profilenamepattern("ESPRESS_");
  EXPECT_EQ((std::vector<DatabaseId>{first, second}), ids(repository->find(query)));
  query.set_profilenamepattern("espresso%");
  EXPECT_EQ((std::vector<DatabaseId>{first, second, third}), ids(repository->find(query)));
  query.set_limit(1);
  query.set_offset(1);
  EXPECT_EQ((std::vector<DatabaseId>{second}), ids(repository->find(query)));
  query.set_offset(0);
  query.set_limit(0);
  query.set_profilenamepattern("");
  EXPECT_TRUE(ids(repository->find(query)).empty());
  query.set_profilenamepattern("' OR 1=1 --");
  EXPECT_TRUE(ids(repository->find(query)).empty());
  *query.mutable_exactprofile() = profile;
  EXPECT_EQ((std::vector<DatabaseId>{first}), ids(repository->find(query)));
  query.mutable_exactprofile()->mutable_steps()->SwapElements(0, 2);
  EXPECT_TRUE(ids(repository->find(query)).empty());
}

TEST_F(BrewHistoryRepositoryTest, DateBoundsAreInclusiveAndCombineWithProfileFilters)
{
  using google::protobuf::util::TimeUtil;

  const auto first = createEntry(makeEntry(100s + 1ms, makeProfile("Other")));
  const auto second = createEntry(makeEntry(100s + 2ms));
  const auto third = createEntry(makeEntry(100s + 3ms));
  FindBrewHistory query;
  *query.mutable_mindate() = TimeUtil::MillisecondsToTimestamp((100s + 2ms).count());
  EXPECT_EQ((std::vector<DatabaseId>{second, third}), ids(repository->find(query)));
  query.clear_mindate();
  *query.mutable_maxdate() = TimeUtil::MillisecondsToTimestamp((100s + 2ms).count());
  EXPECT_EQ((std::vector<DatabaseId>{first, second}), ids(repository->find(query)));
  *query.mutable_mindate() = query.maxdate();
  EXPECT_EQ((std::vector<DatabaseId>{second}), ids(repository->find(query)));
  query.set_profilenamepattern("Other");
  EXPECT_TRUE(ids(repository->find(query)).empty());
  *query.mutable_exactprofile() = makeProfile();
  EXPECT_EQ((std::vector<DatabaseId>{second}), ids(repository->find(query)));
  *query.mutable_mindate() = TimeUtil::MillisecondsToTimestamp((100s + 3ms).count());
  EXPECT_TRUE(ids(repository->find(query)).empty());
}

TEST_F(BrewHistoryRepositoryTest, DatesBeforeUnixEpochRemainSigned)
{
  const auto entry = makeEntry(-1ms);
  const auto id = createEntry(entry);
  expectEntry(entry, repository->get(id));
  FindBrewHistory query;
  *query.mutable_maxdate() = google::protobuf::util::TimeUtil::MillisecondsToTimestamp(-1);
  const auto result = repository->find(query);
  ASSERT_EQ(1, result.entries_size());
  EXPECT_EQ(-1, result.entries(0).startpoint().seconds());
  EXPECT_EQ(999'000'000, result.entries(0).startpoint().nanos());
}

TEST_F(BrewHistoryRepositoryTest, PreservesEntryAndHistoricalProfileIdsAbove32Bits)
{
  sql("INSERT INTO sqlite_sequence(name, seq) VALUES ('profiles', 4294967295), "
      "('brew_history_entries', "
      "4294967295)");
  const auto first = createEntry(makeEntry());
  const auto second = createEntry(makeEntry());
  ASSERT_GT(first, std::numeric_limits<uint32_t>::max());
  auto storage = makeStorage(path);
  const auto profileId = storage.get<BrewHistoryEntryRow>(first).profileId;
  ASSERT_TRUE(profileId.has_value());
  EXPECT_GT(*profileId, std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(profileId, storage.get<BrewHistoryEntryRow>(second).profileId);
  expectEntry(makeEntry(), repository->get(first));
  EXPECT_EQ(makeProfile().SerializeAsString(), repository->get(first).profile.SerializeAsString());
  EXPECT_EQ((std::vector<DatabaseId>{first, second}), ids(repository->find(FindBrewHistory{})));
  repository->erase(first);
  expectCounts(1, 1, 3, 3);
  repository->erase(second);
  expectCounts(0, 0, 0, 0);
}

} // namespace

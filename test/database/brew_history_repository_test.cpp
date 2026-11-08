#include "database/brew_history/repository.hpp"

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

CompletedBrewLog makeLog(std::chrono::nanoseconds start = 100s + 123ms, BrewProfile profile = makeProfile())
{
  return {
    .brewStartTime = std::chrono::system_clock::time_point{start},
    .samplingInterval = 10ms,
    .profile = std::move(profile),
    .records = {{8'500, -200, -300, 2}, {9'000, 36'000, 2'100, 0}, {7'000, 40'000, 100, 1}},
  };
}

std::vector<uint64_t> ids(const BrewLogsSearchResult& result)
{
  std::vector<uint64_t> resultIds;
  for (const auto& log : result.logs()) {
    resultIds.push_back(log.id());
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

  uint64_t createLog(const CompletedBrewLog& log)
  {
    const auto id = repository->createEmptyLog();
    repository->completeLog(id, log);
    return id;
  }

  void expectLog(const CompletedBrewLog& expected, const CompletedBrewLog& actual)
  {
    EXPECT_EQ(expected.profile.SerializeAsString(), actual.profile.SerializeAsString());
    EXPECT_EQ(expected.brewStartTime, actual.brewStartTime);
    EXPECT_EQ(expected.samplingInterval, actual.samplingInterval);
    ASSERT_EQ(expected.records.size(), actual.records.size());
    for (size_t i = 0; i < expected.records.size(); ++i) {
      EXPECT_EQ(expected.records[i].pressure, actual.records[i].pressure);
      EXPECT_EQ(expected.records[i].weight, actual.records[i].weight);
      EXPECT_EQ(expected.records[i].flowRate, actual.records[i].flowRate);
      EXPECT_EQ(expected.records[i].step, actual.records[i].step);
    }
  }

  void expectCounts(int logs, int profiles, int records, int steps)
  {
    auto storage = makeStorage(path);
    EXPECT_EQ(logs, storage.count<BrewLogRow>());
    EXPECT_EQ(profiles, storage.count<HistoricalProfileRow>());
    EXPECT_EQ(records, storage.count<BrewLogRecordRow>());
    EXPECT_EQ(steps, storage.count<StepRow>());
  }

  std::filesystem::path directory;
  std::string path;
  std::unique_ptr<BrewHistoryRepository> repository;
};

TEST_F(BrewHistoryRepositoryTest, EmptyDatabaseAndMissingIds)
{
  EXPECT_TRUE(repository->find(FindBrewLogs{}).logs().empty());
  EXPECT_THROW(repository->get(42), std::system_error);
  EXPECT_THROW(repository->completeLog(42, makeLog()), std::system_error);
  EXPECT_THROW(repository->erase(42), std::system_error);
  EXPECT_THROW(repository->discardIncomplete(42), std::system_error);
  EXPECT_NO_THROW(repository->discardIncomplete());
  expectCounts(0, 0, 0, 0);
}

TEST_F(BrewHistoryRepositoryTest, EmptyLogsReservePersistentIdsWithoutCreatingProfiles)
{
  const auto first = repository->createEmptyLog();
  const auto second = repository->createEmptyLog();
  EXPECT_GT(first, 0U);
  EXPECT_GT(second, first);
  expectCounts(2, 0, 0, 0);
  EXPECT_THROW(repository->get(first), std::system_error);
  EXPECT_TRUE(repository->find(FindBrewLogs{}).logs().empty());
  repository.reset();
  repository = std::make_unique<BrewHistoryRepository>(path);
  EXPECT_THROW(repository->get(first), std::system_error);
  repository->completeLog(first, makeLog());
  expectLog(makeLog(), repository->get(first));
  expectCounts(2, 1, 3, 3);
  repository->discardIncomplete(second);
  expectCounts(1, 1, 3, 3);
  EXPECT_THROW(repository->completeLog(second, makeLog()), std::system_error);
  repository->erase(first);
  const auto empty = repository->createEmptyLog();
  repository->discardIncomplete(empty);
  expectCounts(0, 0, 0, 0);
}

TEST_F(BrewHistoryRepositoryTest, ErasePreservesIncompleteLogsAndTheyCanStillBeCompleted)
{
  const auto incomplete = repository->createEmptyLog();
  const auto complete = createLog(makeLog());

  EXPECT_THROW(repository->erase(incomplete), std::system_error);
  expectCounts(2, 1, 3, 3);
  EXPECT_THROW(repository->get(incomplete), std::system_error);
  EXPECT_EQ((std::vector<uint64_t>{complete}), ids(repository->find(FindBrewLogs{})));

  repository->completeLog(incomplete, makeLog());
  expectLog(makeLog(), repository->get(incomplete));
  repository->erase(incomplete);
  expectCounts(1, 1, 3, 3);
}

TEST_F(BrewHistoryRepositoryTest, DiscardIncompletePreservesCompletedLogsAndTheirData)
{
  const auto incomplete = repository->createEmptyLog();
  const auto otherIncomplete = repository->createEmptyLog();
  const auto complete = createLog(makeLog());
  const auto original = repository->find(FindBrewLogs{});

  EXPECT_THROW(repository->discardIncomplete(complete), std::system_error);
  expectCounts(3, 1, 3, 3);
  EXPECT_EQ(original.SerializeAsString(), repository->find(FindBrewLogs{}).SerializeAsString());

  repository->discardIncomplete(incomplete);
  expectCounts(2, 1, 3, 3);
  EXPECT_THROW(repository->completeLog(incomplete, makeLog()), std::system_error);
  EXPECT_THROW(repository->discardIncomplete(incomplete), std::system_error);
  repository->completeLog(otherIncomplete, makeLog());
  expectCounts(2, 1, 6, 3);
}

TEST_F(BrewHistoryRepositoryTest, DiscardAllIncompletePreservesCompletedLogsAndTheirData)
{
  const auto firstIncomplete = repository->createEmptyLog();
  createLog(makeLog());
  const auto secondIncomplete = repository->createEmptyLog();
  createLog(makeLog(200s));
  auto emptyLog = makeLog(300s, makeProfile("Other"));
  emptyLog.records.clear();
  createLog(emptyLog);
  const auto original = repository->find(FindBrewLogs{});
  expectCounts(5, 2, 6, 6);

  repository->discardIncomplete();
  expectCounts(3, 2, 6, 6);
  EXPECT_EQ(original.SerializeAsString(), repository->find(FindBrewLogs{}).SerializeAsString());
  EXPECT_THROW(repository->completeLog(firstIncomplete, makeLog()), std::system_error);
  EXPECT_THROW(repository->completeLog(secondIncomplete, makeLog()), std::system_error);

  EXPECT_NO_THROW(repository->discardIncomplete());
  expectCounts(3, 2, 6, 6);
  EXPECT_EQ(original.SerializeAsString(), repository->find(FindBrewLogs{}).SerializeAsString());
}

TEST_F(BrewHistoryRepositoryTest, DiscardAllIncompleteClearsReservationsAndPreservesIdSequence)
{
  const auto first = repository->createEmptyLog();
  const auto second = repository->createEmptyLog();

  repository->discardIncomplete();
  expectCounts(0, 0, 0, 0);
  EXPECT_THROW(repository->completeLog(first, makeLog()), std::system_error);
  EXPECT_THROW(repository->completeLog(second, makeLog()), std::system_error);

  const auto next = repository->createEmptyLog();
  EXPECT_GT(next, second);
  repository->completeLog(next, makeLog());
  expectLog(makeLog(), repository->get(next));
  expectCounts(1, 1, 3, 3);
}

TEST_F(BrewHistoryRepositoryTest, CompletingAnAlreadyCompletedLogLeavesHistoryUnchanged)
{
  const auto id = createLog(makeLog());
  const auto original = repository->find(FindBrewLogs{});
  EXPECT_THROW(repository->completeLog(id, makeLog(200s, makeProfile("Replacement"))), std::logic_error);
  EXPECT_EQ(original.SerializeAsString(), repository->find(FindBrewLogs{}).SerializeAsString());
  expectCounts(1, 1, 3, 3);
  EXPECT_NO_THROW(createLog(makeLog()));
  expectCounts(2, 1, 6, 3);
}

TEST_F(BrewHistoryRepositoryTest, RoundTripsAllFieldsAndFindReturnsCompleteLogs)
{
  const auto profile = makeProfile("Café 'special' ☕");
  const auto log = makeLog(100s + 123ms, profile);
  const auto id = createLog(log);
  const auto storedLog = repository->get(id);
  EXPECT_GT(id, 0U);
  expectLog(log, storedLog);

  const auto result = repository->find(FindBrewLogs{});
  ASSERT_EQ(1, result.logs_size());
  const auto& found = result.logs(0);
  EXPECT_EQ(id, found.id());
  EXPECT_EQ(profile.SerializeAsString(), found.profile().SerializeAsString());
  EXPECT_EQ(100, found.startpoint().seconds());
  EXPECT_EQ(123'000'000, found.startpoint().nanos());
  EXPECT_EQ(0, found.samplinginterval().seconds());
  EXPECT_EQ(10'000'000, found.samplinginterval().nanos());
  ASSERT_EQ(log.records.size(), found.records_size());
  for (size_t i = 0; i < log.records.size(); ++i) {
    const auto& record = found.records(static_cast<int>(i));
    EXPECT_EQ(log.records[i].pressure, record.pressure());
    EXPECT_EQ(log.records[i].weight, record.weight());
    EXPECT_EQ(log.records[i].flowRate, record.flowrate());
    EXPECT_EQ(log.records[i].step, record.stepindex());
  }
}

TEST_F(BrewHistoryRepositoryTest, StoresTimesInMillisecondsAndTruncatesSubmillisecondPrecision)
{
  auto log = makeLog(100s + 123ms + 456us);
  log.samplingInterval = 10ms + 321us;
  const auto id = createLog(log);
  auto storage = makeStorage(path);
  const auto row = storage.get<BrewLogRow>(id);
  EXPECT_EQ(100'123, row.startTime);
  EXPECT_EQ(10, row.samplingInterval);
  expectLog(makeLog(), repository->get(id));
  const auto result = repository->find(FindBrewLogs{});
  ASSERT_EQ(1, result.logs_size());
  EXPECT_EQ(123'000'000, result.logs(0).startpoint().nanos());
  EXPECT_EQ(10'000'000, result.logs(0).samplinginterval().nanos());
}

TEST_F(BrewHistoryRepositoryTest, EmptyLogsAndAbsentAndDefaultConditionsRemainDistinct)
{
  auto log = makeLog();
  log.profile.Clear();
  log.profile.add_steps();
  log.records.clear();
  const auto absent = createLog(log);
  EXPECT_EQ(log.profile.SerializeAsString(), repository->get(absent).profile.SerializeAsString());
  EXPECT_TRUE(repository->get(absent).records.empty());
  log.profile.mutable_stopcondition();
  const auto stop = createLog(log);
  EXPECT_EQ(log.profile.SerializeAsString(), repository->get(stop).profile.SerializeAsString());
  log.profile.mutable_steps(0)->mutable_advancecondition();
  const auto advance = createLog(log);
  EXPECT_EQ(log.profile.SerializeAsString(), repository->get(advance).profile.SerializeAsString());
  log.profile.clear_steps();
  const auto noSteps = createLog(log);
  EXPECT_EQ(log.profile.SerializeAsString(), repository->get(noSteps).profile.SerializeAsString());
  expectCounts(4, 4, 0, 3);
}

TEST_F(BrewHistoryRepositoryTest, ReusesProfilesUntilLastReferenceIsDeleted)
{
  const auto profile = makeProfile();
  const auto first = createLog(makeLog(100s + 123ms, profile));
  const auto second = createLog(makeLog(200s, profile));
  const auto other = createLog(makeLog(100s + 123ms, makeProfile("Other")));
  expectCounts(3, 2, 9, 6);
  auto storage = makeStorage(path);
  const auto profileId = storage.get<BrewLogRow>(first).profileId;
  EXPECT_EQ(profileId, storage.get<BrewLogRow>(second).profileId);
  repository->erase(first);
  expectCounts(2, 2, 6, 6);
  EXPECT_THROW(repository->get(first), std::system_error);
  EXPECT_EQ(profile.SerializeAsString(), repository->get(second).profile.SerializeAsString());
  repository->erase(second);
  expectCounts(1, 1, 3, 3);
  EXPECT_FALSE(storage.get_optional<HistoricalProfileRow>(profileId.value()));
  EXPECT_EQ("Other", repository->get(other).profile.name());
  EXPECT_THROW(repository->erase(second), std::system_error);
  const auto recreated = createLog(makeLog(100s + 123ms, profile));
  EXPECT_NE(profileId, storage.get<BrewLogRow>(recreated).profileId);
  expectCounts(2, 2, 6, 6);
}

TEST_F(BrewHistoryRepositoryTest, FingerprintDistinguishesEveryFieldAndStepOrder)
{
  const auto original = makeProfile();
  createLog(makeLog(100s + 123ms, original));
  int profiles = 1;
  auto distinct = [&](const char* field, auto mutate) {
    SCOPED_TRACE(field);
    auto profile = original;
    mutate(profile);
    const auto id = createLog(makeLog(100s + 123ms, profile));
    EXPECT_EQ(profile.SerializeAsString(), repository->get(id).profile.SerializeAsString());
    auto storage = makeStorage(path);
    EXPECT_EQ(++profiles, storage.count<HistoricalProfileRow>());
    createLog(makeLog(100s + 123ms, profile));
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
  const auto id = createLog(makeLog(100s + 123ms, profile));
  const auto first = repository->createEmptyLog();
  const auto second = repository->createEmptyLog();
  sql("CREATE TRIGGER fail_step BEFORE INSERT ON steps WHEN NEW.position = 1 "
      "BEGIN SELECT RAISE(ABORT, 'injected step failure'); END");
  EXPECT_THROW(repository->completeLog(first, makeLog(100s + 123ms, makeProfile("Rejected"))),
               std::system_error);
  expectCounts(3, 1, 3, 3);
  sql("DROP TRIGGER fail_step");
  sql("CREATE TRIGGER fail_record BEFORE INSERT ON brew_log_records WHEN NEW.position = 1 "
      "BEGIN SELECT RAISE(ABORT, 'injected record failure'); END");
  EXPECT_THROW(repository->completeLog(first, makeLog(100s + 123ms, makeProfile("Rejected"))),
               std::system_error);
  EXPECT_THROW(repository->completeLog(second, makeLog(100s + 123ms, profile)), std::system_error);
  expectCounts(3, 1, 3, 3);
  auto storage = makeStorage(path);
  for (const auto pending : {first, second}) {
    const auto row = storage.get<BrewLogRow>(pending);
    EXPECT_FALSE(row.profileId.has_value());
    EXPECT_EQ(0, row.startTime);
    EXPECT_EQ(0, row.samplingInterval);
    EXPECT_THROW(repository->get(pending), std::system_error);
  }
  EXPECT_EQ((std::vector<uint64_t>{id}), ids(repository->find(FindBrewLogs{})));
  expectLog(makeLog(), repository->get(id));
  sql("DROP TRIGGER fail_record");
  EXPECT_NO_THROW(repository->completeLog(first, makeLog(100s + 123ms, makeProfile("Accepted"))));
  EXPECT_NO_THROW(repository->completeLog(second, makeLog(100s + 123ms, profile)));
  expectCounts(3, 2, 9, 6);
  expectLog(makeLog(100s + 123ms, makeProfile("Accepted")), repository->get(first));
  expectLog(makeLog(), repository->get(second));
}

TEST_F(BrewHistoryRepositoryTest, FailedProfileCleanupRestoresLogAndRecords)
{
  const auto id = createLog(makeLog());
  sql("CREATE TRIGGER fail_cleanup BEFORE DELETE ON profiles "
      "BEGIN SELECT RAISE(ABORT, 'injected cleanup failure'); END");
  EXPECT_THROW(repository->erase(id), std::system_error);
  expectCounts(1, 1, 3, 3);
  expectLog(makeLog(), repository->get(id));
  sql("DROP TRIGGER fail_cleanup");
  repository->erase(id);
  expectCounts(0, 0, 0, 0);
}

TEST_F(BrewHistoryRepositoryTest, ReopeningPreservesLogsAndProfileReferences)
{
  const auto first = createLog(makeLog());
  const auto second = createLog(makeLog(200s));
  repository->erase(first);
  const auto expected = repository->find(FindBrewLogs{});
  repository.reset();
  repository = std::make_unique<BrewHistoryRepository>(path);
  EXPECT_EQ(expected.SerializeAsString(), repository->find(FindBrewLogs{}).SerializeAsString());
  EXPECT_THROW(repository->get(first), std::system_error);
  expectLog(makeLog(200s), repository->get(second));
  createLog(makeLog());
  expectCounts(2, 1, 6, 3);
}

TEST_F(BrewHistoryRepositoryTest, PaginationUsesIdOrderAndZeroLimitMeansUnlimited)
{
  repository->createEmptyLog();
  const auto first = createLog(makeLog(300s));
  repository->createEmptyLog();
  const auto second = createLog(makeLog(100s));
  const auto third = createLog(makeLog(200s));
  FindBrewLogs query;
  EXPECT_EQ((std::vector<uint64_t>{first, second, third}), ids(repository->find(query)));
  query.set_offset(1);
  EXPECT_EQ((std::vector<uint64_t>{second, third}), ids(repository->find(query)));
  query.set_limit(1);
  EXPECT_EQ((std::vector<uint64_t>{second}), ids(repository->find(query)));
  query.set_offset(3);
  EXPECT_TRUE(ids(repository->find(query)).empty());
}

TEST_F(BrewHistoryRepositoryTest, ProfileFiltersApplyBeforePagination)
{
  createLog(makeLog(100s + 123ms, makeProfile("Other")));
  const auto profile = makeProfile();
  const auto first = createLog(makeLog(100s + 123ms, profile));
  auto changed = profile;
  changed.set_temperature(94'000);
  const auto second = createLog(makeLog(100s + 123ms, changed));
  const auto third = createLog(makeLog(100s + 123ms, makeProfile("espresso long")));
  FindBrewLogs query;
  query.set_profilenamepattern("ESPRESS_");
  EXPECT_EQ((std::vector<uint64_t>{first, second}), ids(repository->find(query)));
  query.set_profilenamepattern("espresso%");
  EXPECT_EQ((std::vector<uint64_t>{first, second, third}), ids(repository->find(query)));
  query.set_limit(1);
  query.set_offset(1);
  EXPECT_EQ((std::vector<uint64_t>{second}), ids(repository->find(query)));
  query.set_offset(0);
  query.set_limit(0);
  query.set_profilenamepattern("");
  EXPECT_TRUE(ids(repository->find(query)).empty());
  query.set_profilenamepattern("' OR 1=1 --");
  EXPECT_TRUE(ids(repository->find(query)).empty());
  *query.mutable_exactprofile() = profile;
  EXPECT_EQ((std::vector<uint64_t>{first}), ids(repository->find(query)));
  query.mutable_exactprofile()->mutable_steps()->SwapElements(0, 2);
  EXPECT_TRUE(ids(repository->find(query)).empty());
}

TEST_F(BrewHistoryRepositoryTest, DateBoundsAreInclusiveAndCombineWithProfileFilters)
{
  using google::protobuf::util::TimeUtil;

  const auto first = createLog(makeLog(100s + 1ms, makeProfile("Other")));
  const auto second = createLog(makeLog(100s + 2ms));
  const auto third = createLog(makeLog(100s + 3ms));
  FindBrewLogs query;
  *query.mutable_mindate() = TimeUtil::MillisecondsToTimestamp((100s + 2ms).count());
  EXPECT_EQ((std::vector<uint64_t>{second, third}), ids(repository->find(query)));
  query.clear_mindate();
  *query.mutable_maxdate() = TimeUtil::MillisecondsToTimestamp((100s + 2ms).count());
  EXPECT_EQ((std::vector<uint64_t>{first, second}), ids(repository->find(query)));
  *query.mutable_mindate() = query.maxdate();
  EXPECT_EQ((std::vector<uint64_t>{second}), ids(repository->find(query)));
  query.set_profilenamepattern("Other");
  EXPECT_TRUE(ids(repository->find(query)).empty());
  *query.mutable_exactprofile() = makeProfile();
  EXPECT_EQ((std::vector<uint64_t>{second}), ids(repository->find(query)));
  *query.mutable_mindate() = TimeUtil::MillisecondsToTimestamp((100s + 3ms).count());
  EXPECT_TRUE(ids(repository->find(query)).empty());
}

TEST_F(BrewHistoryRepositoryTest, DatesBeforeUnixEpochRemainSigned)
{
  const auto log = makeLog(-1ms);
  const auto id = createLog(log);
  expectLog(log, repository->get(id));
  FindBrewLogs query;
  *query.mutable_maxdate() = google::protobuf::util::TimeUtil::MillisecondsToTimestamp(-1);
  const auto result = repository->find(query);
  ASSERT_EQ(1, result.logs_size());
  EXPECT_EQ(-1, result.logs(0).startpoint().seconds());
  EXPECT_EQ(999'000'000, result.logs(0).startpoint().nanos());
}

TEST_F(BrewHistoryRepositoryTest, PreservesLogAndHistoricalProfileIdsAbove32Bits)
{
  sql("INSERT INTO sqlite_sequence(name, seq) VALUES ('profiles', 4294967295), ('brew_logs', "
      "4294967295)");
  const auto first = createLog(makeLog());
  const auto second = createLog(makeLog());
  ASSERT_GT(first, std::numeric_limits<uint32_t>::max());
  auto storage = makeStorage(path);
  const auto profileId = storage.get<BrewLogRow>(first).profileId;
  ASSERT_TRUE(profileId.has_value());
  EXPECT_GT(*profileId, std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(profileId, storage.get<BrewLogRow>(second).profileId);
  expectLog(makeLog(), repository->get(first));
  EXPECT_EQ(makeProfile().SerializeAsString(), repository->get(first).profile.SerializeAsString());
  EXPECT_EQ((std::vector<uint64_t>{first, second}), ids(repository->find(FindBrewLogs{})));
  repository->erase(first);
  expectCounts(1, 1, 3, 3);
  repository->erase(second);
  expectCounts(0, 0, 0, 0);
}

} // namespace

#include "database/brew_profiles/repository.hpp"
#include "database/steps_schema.hpp"

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

#include <gtest/gtest.h>

using namespace openpressod;
using namespace openpressod::db::brew_profiles;

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
      step->mutable_advancecondition()->set_type(i == 0 ? BrewStepAdvanceConditionType_StepTime
                                                        : BrewStepAdvanceConditionType_TotalTime);
      step->mutable_advancecondition()->set_value(5.5F + i);
    }
  }
  return profile;
}

std::vector<uint64_t> ids(const BrewProfilesSearchResult& result)
{
  std::vector<uint64_t> resultIds;
  for (const auto& profile : result.profiles()) {
    resultIds.push_back(profile.id());
  }
  return resultIds;
}

class BrewProfilesRepositoryTest : public testing::Test {
protected:
  void SetUp() override
  {
    // Atomically reserve a directory, including when tests run in parallel processes.
    auto base = std::filesystem::temp_directory_path() /
                ("openpressod-profiles-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    directory = base;
    for (unsigned suffix = 0; !std::filesystem::create_directory(directory); ++suffix) {
      directory = base.string() + "-" + std::to_string(suffix);
    }
    path = (directory / "profiles.sqlite").string();
    repository = std::make_unique<BrewProfilesRepository>(path);
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

  std::filesystem::path directory;
  std::string path;
  std::unique_ptr<BrewProfilesRepository> repository;
};

TEST_F(BrewProfilesRepositoryTest, EmptyDatabaseAndMissingIds)
{
  EXPECT_EQ(0, repository->find(FindBrewProfiles{}).profiles_size());
  EXPECT_THROW(repository->get(42), std::system_error);
  EXPECT_THROW(repository->update(42, makeProfile()), std::system_error);
  EXPECT_THROW(repository->erase(42), std::system_error);
  EXPECT_EQ(0, repository->find(FindBrewProfiles{}).profiles_size());
}

TEST_F(BrewProfilesRepositoryTest, CreateRoundTripsAllFieldsAndOrderedSteps)
{
  auto profile = makeProfile("Café 'special' ☕");
  const auto before = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  const auto id = repository->create(profile);
  auto stored = repository->get(id);
  const auto after = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  EXPECT_GT(id, 0U);
  EXPECT_EQ(id, stored.id());
  EXPECT_EQ(profile.SerializeAsString(), stored.profile().SerializeAsString());
  EXPECT_GE(stored.created().seconds(), before);
  EXPECT_LE(stored.created().seconds(), after);
  EXPECT_EQ(stored.created().seconds(), stored.modified().seconds());
  EXPECT_EQ(0, stored.created().nanos());
  EXPECT_EQ(0, stored.modified().nanos());
  auto found = repository->find(FindBrewProfiles{});
  ASSERT_EQ(1, found.profiles_size());
  EXPECT_EQ(stored.SerializeAsString(), found.profiles(0).SerializeAsString());
}

TEST_F(BrewProfilesRepositoryTest, FindByProfileReturnsCompletePersistentProfile)
{
  repository->create(makeProfile("Other"));
  const auto profile = makeProfile("Café 'special' ☕");
  const auto id = repository->create(profile);
  sql("UPDATE profiles SET created_at = 100, modified_at = 200");
  const auto expected = repository->get(id);
  const auto found = repository->find(profile);
  EXPECT_EQ(expected.SerializeAsString(), found.SerializeAsString());
  EXPECT_EQ(profile.SerializeAsString(), found.profile().SerializeAsString());
  EXPECT_EQ(expected.SerializeAsString(), repository->find(profile).SerializeAsString());
}

TEST_F(BrewProfilesRepositoryTest, FindByProfileThrowsWhenMissingAndRepositoryRemainsUsable)
{
  const auto profile = makeProfile();
  EXPECT_THROW(repository->find(profile), std::system_error);
  const auto id = repository->create(profile);
  EXPECT_EQ(id, repository->find(profile).id());
  repository->erase(id);
  EXPECT_THROW(repository->find(profile), std::system_error);
  const auto recreated = repository->create(profile);
  EXPECT_EQ(recreated, repository->find(profile).id());
}

TEST_F(BrewProfilesRepositoryTest, FindByProfileRequiresEveryFieldAndStepOrderToMatch)
{
  const auto profile = makeProfile();
  const auto id = repository->create(profile);
  auto expectMissing = [&](const char* field, auto mutate) {
    SCOPED_TRACE(field);
    auto changed = profile;
    mutate(changed);
    EXPECT_THROW(repository->find(changed), std::system_error);
  };
  expectMissing("name", [](auto& p) { p.set_name("espresso"); });
  expectMissing("temperature", [](auto& p) { p.set_temperature(94'000); });
  expectMissing("stop condition presence", [](auto& p) { p.clear_stopcondition(); });
  expectMissing("stop condition type",
                [](auto& p) { p.mutable_stopcondition()->set_type(StopConditionType_TotalTime); });
  expectMissing("stop condition value", [](auto& p) { p.mutable_stopcondition()->set_value(37.5F); });
  expectMissing("step count", [](auto& p) { p.mutable_steps()->RemoveLast(); });
  expectMissing("step order", [](auto& p) { p.mutable_steps()->SwapElements(0, 2); });
  expectMissing("step target type",
                [](auto& p) { p.mutable_steps(0)->set_targettype(BrewStepTargetType_FlowRate); });
  expectMissing("step target value", [](auto& p) { p.mutable_steps(0)->set_targetvalue(9.5F); });
  expectMissing("advance condition presence",
                [](auto& p) { p.mutable_steps(0)->clear_advancecondition(); });
  expectMissing("advance condition type", [](auto& p) {
    p.mutable_steps(0)->mutable_advancecondition()->set_type(BrewStepAdvanceConditionType_TotalWeight);
  });
  expectMissing("advance condition value",
                [](auto& p) { p.mutable_steps(0)->mutable_advancecondition()->set_value(10.5F); });
  EXPECT_EQ(id, repository->find(profile).id());
}

TEST_F(BrewProfilesRepositoryTest, FindByProfileDistinguishesAbsentAndDefaultConditions)
{
  BrewProfile profile;
  profile.set_name("Minimal");
  profile.add_steps();
  const auto id = repository->create(profile);
  EXPECT_EQ(profile.SerializeAsString(), repository->find(profile).profile().SerializeAsString());

  auto withStop = profile;
  withStop.mutable_stopcondition();
  EXPECT_THROW(repository->find(withStop), std::system_error);
  auto withAdvance = profile;
  withAdvance.mutable_steps(0)->mutable_advancecondition();
  EXPECT_THROW(repository->find(withAdvance), std::system_error);

  repository->update(id, withStop);
  EXPECT_THROW(repository->find(profile), std::system_error);
  EXPECT_EQ(withStop.SerializeAsString(), repository->find(withStop).profile().SerializeAsString());
  repository->update(id, withAdvance);
  EXPECT_THROW(repository->find(withStop), std::system_error);
  EXPECT_EQ(withAdvance.SerializeAsString(), repository->find(withAdvance).profile().SerializeAsString());
}

TEST_F(BrewProfilesRepositoryTest, AbsentAndPresentDefaultConditionsRemainDistinct)
{
  BrewProfile profile;
  profile.set_name("Minimal");
  auto* step = profile.add_steps();
  step->set_targetvalue(0);
  const auto id = repository->create(profile);
  auto stored = repository->get(id).profile();
  EXPECT_FALSE(stored.has_stopcondition());
  ASSERT_EQ(1, stored.steps_size());
  EXPECT_FALSE(stored.steps(0).has_advancecondition());
  profile.mutable_stopcondition();
  profile.mutable_steps(0)->mutable_advancecondition();
  repository->update(id, profile);
  EXPECT_EQ(profile.SerializeAsString(), repository->get(id).profile().SerializeAsString());
}

TEST_F(BrewProfilesRepositoryTest, UpdateReplacesStepsConditionsAndMetadata)
{
  const auto id = repository->create(makeProfile());
  sql("UPDATE profiles SET created_at = 100, modified_at = 200");
  auto replacement = makeProfile("Ristretto");
  replacement.set_temperature(91'000);
  replacement.clear_stopcondition();
  replacement.clear_steps();
  replacement.add_steps()->set_targettype(BrewStepTargetType_FlowRate);
  repository->update(id, replacement);
  auto stored = repository->get(id);
  EXPECT_EQ(id, stored.id());
  EXPECT_EQ(100, stored.created().seconds());
  EXPECT_GT(stored.modified().seconds(), 200);
  EXPECT_EQ(replacement.SerializeAsString(), stored.profile().SerializeAsString());
  replacement.clear_steps();
  repository->update(id, replacement);
  EXPECT_EQ(0, repository->get(id).profile().steps_size());
  repository->update(id, makeProfile());
  EXPECT_EQ(makeProfile().SerializeAsString(), repository->get(id).profile().SerializeAsString());
}

TEST_F(BrewProfilesRepositoryTest, RejectsEmptyAndDuplicateNamesWithoutChangingExistingProfiles)
{
  auto profile = makeProfile();
  const auto id = repository->create(profile);
  EXPECT_THROW(repository->create(profile), std::system_error);
  profile.set_temperature(94'000);
  EXPECT_THROW(repository->create(profile), std::system_error);
  profile.set_name("");
  EXPECT_THROW(repository->create(profile), std::system_error);
  EXPECT_THROW(repository->update(id, profile), std::system_error);
  EXPECT_EQ(1, repository->find(FindBrewProfiles{}).profiles_size());
  EXPECT_EQ(makeProfile().SerializeAsString(), repository->get(id).profile().SerializeAsString());
}

TEST_F(BrewProfilesRepositoryTest, ConflictingUpdateLeavesBothProfilesUnchanged)
{
  const auto first = repository->create(makeProfile("First"));
  const auto second = repository->create(makeProfile("Second"));
  const auto original = repository->get(second);
  EXPECT_THROW(repository->update(second, makeProfile("First")), std::system_error);
  EXPECT_EQ(original.SerializeAsString(), repository->get(second).SerializeAsString());
  EXPECT_EQ("First", repository->get(first).profile().name());
}

TEST_F(BrewProfilesRepositoryTest, FailedStepWritesRollBackCreateAndUpdate)
{
  const auto id = repository->create(makeProfile());
  const auto original = repository->get(id);
  // Fail after one step has been written, exercising rollback of parent and child rows.
  sql("CREATE TRIGGER fail_step BEFORE INSERT ON steps WHEN NEW.position = 1 "
      "BEGIN SELECT RAISE(ABORT, 'injected step failure'); END");
  EXPECT_THROW(repository->create(makeProfile("Rejected")), std::system_error);
  EXPECT_THROW(repository->update(id, makeProfile("Replacement")), std::system_error);
  EXPECT_EQ(1, repository->find(FindBrewProfiles{}).profiles_size());
  EXPECT_EQ(original.SerializeAsString(), repository->get(id).SerializeAsString());
  EXPECT_EQ(original.SerializeAsString(), repository->find(makeProfile()).SerializeAsString());
  EXPECT_THROW(repository->find(makeProfile("Rejected")), std::system_error);
  EXPECT_THROW(repository->find(makeProfile("Replacement")), std::system_error);
  sql("DROP TRIGGER fail_step");
  EXPECT_NO_THROW(repository->create(makeProfile("Accepted")));
  EXPECT_NO_THROW(repository->update(id, makeProfile("Replacement")));
}

TEST_F(BrewProfilesRepositoryTest, EraseCascadesStepsAndPreservesOtherProfiles)
{
  using openpressod::db::StepRow;

  const auto first = repository->create(makeProfile("First"));
  const auto second = repository->create(makeProfile("Second"));
  repository->erase(first);
  EXPECT_THROW(repository->erase(first), std::system_error);
  EXPECT_THROW(repository->get(first), std::system_error);
  EXPECT_EQ((std::vector<uint64_t>{second}), ids(repository->find(FindBrewProfiles{})));
  auto storage = makeStorage(path);
  EXPECT_EQ(0, storage.count<StepRow>(sqlite_orm::where(sqlite_orm::c(&StepRow::profileId) == first)));
  EXPECT_EQ(3, storage.count<StepRow>(sqlite_orm::where(sqlite_orm::c(&StepRow::profileId) == second)));
  const auto recreated = repository->create(makeProfile("First"));
  EXPECT_GT(recreated, second);
  EXPECT_EQ(makeProfile("First").SerializeAsString(),
            repository->get(recreated).profile().SerializeAsString());
}

TEST_F(BrewProfilesRepositoryTest, ReopeningDatabasePreservesUpdatesAndDeletions)
{
  const auto first = repository->create(makeProfile("First"));
  const auto second = repository->create(makeProfile("Second"));
  repository->update(second, makeProfile("Updated"));
  repository->erase(first);
  auto original = repository->get(second);
  repository.reset();
  repository = std::make_unique<BrewProfilesRepository>(path);
  EXPECT_EQ(original.SerializeAsString(), repository->get(second).SerializeAsString());
  EXPECT_EQ(original.SerializeAsString(), repository->find(makeProfile("Updated")).SerializeAsString());
  EXPECT_THROW(repository->find(makeProfile("Second")), std::system_error);
  EXPECT_THROW(repository->find(makeProfile("First")), std::system_error);
  EXPECT_THROW(repository->get(first), std::system_error);
  EXPECT_EQ((std::vector<uint64_t>{second}), ids(repository->find(FindBrewProfiles{})));
}

TEST_F(BrewProfilesRepositoryTest, PaginationUsesIdOrderAndZeroLimitMeansUnlimited)
{
  const auto first = repository->create(makeProfile("Zebra"));
  const auto second = repository->create(makeProfile("Alpha"));
  const auto third = repository->create(makeProfile("Middle"));
  FindBrewProfiles query;
  EXPECT_EQ((std::vector<uint64_t>{first, second, third}), ids(repository->find(query)));
  query.set_offset(1);
  EXPECT_EQ((std::vector<uint64_t>{second, third}), ids(repository->find(query)));
  query.set_limit(1);
  EXPECT_EQ((std::vector<uint64_t>{second}), ids(repository->find(query)));
  query.set_offset(3);
  EXPECT_TRUE(ids(repository->find(query)).empty());
}

TEST_F(BrewProfilesRepositoryTest, NamePatternsUseSqlLikeAndApplyBeforePagination)
{
  repository->create(makeProfile("Other"));
  const auto first = repository->create(makeProfile("Espresso"));
  const auto second = repository->create(makeProfile("espresso long"));
  FindBrewProfiles query;
  query.set_namepattern("ESPRESS_");
  EXPECT_EQ((std::vector<uint64_t>{first}), ids(repository->find(query)));
  query.set_namepattern("espresso%");
  EXPECT_EQ((std::vector<uint64_t>{first, second}), ids(repository->find(query)));
  query.set_limit(1);
  query.set_offset(1);
  EXPECT_EQ((std::vector<uint64_t>{second}), ids(repository->find(query)));
  query.set_offset(0);
  query.set_namepattern("");
  EXPECT_TRUE(ids(repository->find(query)).empty());
  query.set_namepattern("' OR 1=1 --");
  EXPECT_TRUE(ids(repository->find(query)).empty());
}

TEST_F(BrewProfilesRepositoryTest, TimestampFiltersAreInclusiveAndCanBeCombined)
{
  const auto first = repository->create(makeProfile("First"));
  const auto second = repository->create(makeProfile("Second"));
  const auto third = repository->create(makeProfile("Third"));
  sql("UPDATE profiles SET created_at = id * 100, modified_at = id * 100 + 50");
  FindBrewProfiles query;
  query.mutable_mincreated()->set_seconds(200);
  EXPECT_EQ((std::vector<uint64_t>{second, third}), ids(repository->find(query)));
  query.clear_mincreated();
  query.mutable_maxcreated()->set_seconds(200);
  EXPECT_EQ((std::vector<uint64_t>{first, second}), ids(repository->find(query)));
  query.clear_maxcreated();
  query.mutable_minmodified()->set_seconds(250);
  EXPECT_EQ((std::vector<uint64_t>{second, third}), ids(repository->find(query)));
  query.clear_minmodified();
  query.mutable_maxmodified()->set_seconds(250);
  EXPECT_EQ((std::vector<uint64_t>{first, second}), ids(repository->find(query)));
  query.mutable_mincreated()->set_seconds(200);
  query.mutable_maxcreated()->set_seconds(200);
  query.mutable_minmodified()->set_seconds(250);
  EXPECT_EQ((std::vector<uint64_t>{second}), ids(repository->find(query)));
  query.set_namepattern("First");
  EXPECT_TRUE(ids(repository->find(query)).empty());
  query.clear_namepattern();
  query.mutable_mincreated()->set_seconds(300);
  EXPECT_TRUE(ids(repository->find(query)).empty());
}

TEST_F(BrewProfilesRepositoryTest, PreservesIdsAbove32BitsInGetAndFind)
{
  sql("INSERT INTO sqlite_sequence(name, seq) VALUES ('profiles', 4294967295)");
  const auto id = repository->create(makeProfile());
  ASSERT_GT(id, std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(id, repository->get(id).id());
  EXPECT_EQ(id, repository->find(makeProfile()).id());
  EXPECT_EQ((std::vector<uint64_t>{id}), ids(repository->find(FindBrewProfiles{})));
  repository->update(id, makeProfile("Updated"));
  EXPECT_EQ(id, repository->get(id).id());
  EXPECT_THROW(repository->find(makeProfile()), std::system_error);
  EXPECT_EQ(id, repository->find(makeProfile("Updated")).id());
  repository->erase(id);
  EXPECT_THROW(repository->find(makeProfile("Updated")), std::system_error);
  EXPECT_THROW(repository->get(id), std::system_error);
}

} // namespace

#include "async_events_dispatcher/async_events_dispatcher.hpp"
#include "brew_history/brew_history_data.hpp"
#include "brew_history/brew_history_manager.hpp"
#include "brew_profiler_mock.hpp"
#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"
#include "database/brew_history/async_repository.hpp"
#include "database/brew_profile_fingerprint.hpp"
#include "database/brew_profiles/async_repository.hpp"
#include "database/database_id.hpp"
#include "leds/leds_handler.hpp"
#include "libopenpresso_core_mock.hpp"
#include "logical_input_mock.hpp"
#include "logical_output_mock.hpp"
#include "pressure_sensor_mock.hpp"
#include "service/openppressod_service_impl.hpp"
#include "state_manager/brew_profile_manager.hpp"
#include "state_manager/state_manager.hpp"
#include "state_manager/status_manager.hpp"
#include "state_manager/user_settings_manager.hpp"
#include "temperature_controller_mock.hpp"
#include "temperature_sensor_mock.hpp"
#include "weight_sensor_mock.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <google/protobuf/util/time_util.h>
#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>
#include <libopenpresso/brew_steps_data.hpp>

using namespace openpressod;
using namespace std::chrono_literals;
using google::protobuf::Empty;
using google::protobuf::util::TimeUtil;
using testing::_;
using testing::NiceMock;
using testing::Return;

class DatabaseRpcTest : public testing::Test {
protected:
  static BrewProfile makeProfile(std::string name = "Espresso")
  {
    BrewProfile profile;
    profile.set_name(std::move(name));
    profile.set_temperature(93'000);
    profile.mutable_stopcondition()->set_type(StopConditionType_TotalWeight);
    profile.mutable_stopcondition()->set_value(36.5F);
    auto* step = profile.add_steps();
    step->set_targettype(BrewStepTargetType_Pressure);
    step->set_targetvalue(2.5F);
    step->mutable_advancecondition()->set_type(BrewStepAdvanceConditionType_StepTime);
    step->mutable_advancecondition()->set_value(5.5F);
    step = profile.add_steps();
    step->set_targettype(BrewStepTargetType_FlowRate);
    step->set_targetvalue(2.25F);
    return profile;
  }

  static CompletedBrewHistoryEntry makeEntry(std::chrono::milliseconds start = 100s + 123ms,
                                             BrewProfile profile = makeProfile())
  {
    return {
      .brewStartTime = std::chrono::system_clock::time_point{start},
      .samplingInterval = 10ms,
      .profile = std::move(profile),
      .samples = {{8'500, -200, -300, 1}, {9'000, 36'000, 2'100, 0}},
    };
  }

  static std::vector<DatabaseId> ids(const BrewProfilesSearchResult& result)
  {
    std::vector<DatabaseId> resultIds;
    for (const auto& profile : result.profiles()) {
      resultIds.push_back(profile.id());
    }
    return resultIds;
  }

  static std::vector<DatabaseId> ids(const BrewHistorySearchResult& result)
  {
    std::vector<DatabaseId> resultIds;
    for (const auto& entry : result.entries()) {
      resultIds.push_back(entry.id());
    }
    return resultIds;
  }

  void SetUp() override
  {
    const auto base = std::filesystem::temp_directory_path() /
                      ("openpressod-service-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    directory = base;
    for (unsigned suffix = 0; !std::filesystem::create_directory(directory); ++suffix) {
      directory = base.string() + "-" + std::to_string(suffix);
    }
    profilesPath = (directory / "profiles.sqlite").string();
    historyPath = (directory / "history.sqlite").string();
    profiles = std::make_shared<db::brew_profiles::AsyncBrewProfilesRepository>(profilesPath);
    history = std::make_shared<db::brew_history::AsyncBrewHistoryRepository>(historyPath);
    auto steamInput = std::make_shared<NiceMock<MockLogicalInput>>();
    auto steamController = std::make_shared<NiceMock<MockTemperatureController>>();
    auto weight = std::make_shared<NiceMock<MockWeightSensor>>();
    auto pressure = std::make_shared<NiceMock<MockPressureSensor>>();
    auto temperature = std::make_shared<NiceMock<MockTemperatureSensor>>();
    auto led = std::make_shared<NiceMock<MockLogicalOutput>>();
    ON_CALL(*core, getLogicalInput(libopenpresso_config_labels::STEAM_BUTTON_LABEL)).WillByDefault(Return(steamInput));
    ON_CALL(*core, getBrewProfiler(libopenpresso_config_labels::BREW_PROFILER_LABEL)).WillByDefault(Return(profiler));
    ON_CALL(*core, getTemperatureController(libopenpresso_config_labels::BREW_TEMPERATURE_CONTROLLER_LABEL))
      .WillByDefault(Return(brewController));
    ON_CALL(*core, getTemperatureController(libopenpresso_config_labels::STEAM_CONTROLLER_LABEL))
      .WillByDefault(Return(steamController));
    ON_CALL(*core, getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL)).WillByDefault(Return(weight));
    ON_CALL(*core, getPressureSensor(libopenpresso_config_labels::PRESSURE_SENSOR_LABEL))
      .WillByDefault(Return(pressure));
    ON_CALL(*core, getTemperatureSensor(libopenpresso_config_labels::TEMPERATURE_SENSOR_LABEL))
      .WillByDefault(Return(temperature));
    ON_CALL(*core, getLogicalOutput(_)).WillByDefault(Return(led));

    auto config =
      OpenpressodConfig::fromString("[global]\ndaemon_data_directory = '" + directory.string() + "'\n");
    auto leds = std::make_unique<LedsHandler>(core);
    auto status = std::make_unique<StatusManager>(core, config, std::move(leds));
    auto state = std::make_unique<StateManager>(core, std::move(status));
    auto brewTimer = state->brewTimer();
    auto profileManager = std::make_unique<BrewProfileManager>(config);
    profileManager->loadDefaultProfile();
    auto settings = std::make_unique<UserSettingsManager>(config);
    settings->loadDefaultSettings();
    historyManager = std::make_shared<BrewHistoryManager>(core, history, config);
    dispatcher = std::make_shared<AsyncEventDispatcher>(
      std::move(state), std::move(profileManager), std::move(settings), historyManager, history);
    service = std::make_unique<OpenpressodServiceImpl>(
      dispatcher, historyManager, core, std::move(brewTimer), profiles, history);
    grpc::ServerBuilder builder;
    builder.RegisterService(service.get());
    server = builder.BuildAndStart();
    ASSERT_NE(server, nullptr);
    stub = OpenpressoDaemon::NewStub(server->InProcessChannel(grpc::ChannelArguments{}));
  }

  void TearDown() override
  {
    stub.reset();
    if (server) {
      server->Shutdown(std::chrono::system_clock::now() + 2s);
      server->Wait();
    }
    server.reset();
    service.reset();
    ON_CALL(*profiler, isActive()).WillByDefault(Return(false));
    dispatcher.reset();
    historyManager.reset();
    profiles.reset();
    history.reset();
    std::error_code error;
    std::filesystem::remove_all(directory, error);
  }

  template <typename Request, typename Response>
  grpc::Status rpc(
    grpc::Status (OpenpressoDaemon::Stub::*method)(grpc::ClientContext*, const Request&, Response*),
    const Request& request,
    Response& response)
  {
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + 2s);
    return (stub.get()->*method)(&context, request, &response);
  }

  void sql(const std::string& path, const std::string& statement)
  {
    sqlite3* connection = nullptr;
    ASSERT_EQ(SQLITE_OK, sqlite3_open(path.c_str(), &connection));
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> guard(connection, sqlite3_close);
    ASSERT_EQ(SQLITE_OK, sqlite3_exec(connection, statement.c_str(), nullptr, nullptr, nullptr))
      << sqlite3_errmsg(connection);
  }

  DatabaseId createProfile(const BrewProfile& profile)
  {
    PersistentId response;
    const auto status = rpc(&OpenpressoDaemon::Stub::addPersistentBrewProfile, profile, response);
    EXPECT_TRUE(status.ok()) << status.error_message();
    return response.id();
  }

  PersistentBrewProfile getProfile(DatabaseId id)
  {
    PersistentId request;
    request.set_id(id);
    PersistentBrewProfile response;
    const auto status = rpc(&OpenpressoDaemon::Stub::getPersistentBrewProfile, request, response);
    EXPECT_TRUE(status.ok()) << status.error_message();
    return response;
  }

  grpc::Status updateProfile(DatabaseId id, const BrewProfile& profile)
  {
    UpdatePersistentProfile request;
    request.set_id(id);
    *request.mutable_profile() = profile;
    Empty response;
    return rpc(&OpenpressoDaemon::Stub::updatePersistentBrewProfile, request, response);
  }

  grpc::Status deleteProfile(DatabaseId id)
  {
    PersistentId request;
    request.set_id(id);
    Empty response;
    return rpc(&OpenpressoDaemon::Stub::deletePersistentBrewProfile, request, response);
  }

  BrewProfilesSearchResult findProfiles(const FindBrewProfiles& query = {})
  {
    BrewProfilesSearchResult response;
    const auto status = rpc(&OpenpressoDaemon::Stub::findPersistentBrewProfiles, query, response);
    EXPECT_TRUE(status.ok()) << status.error_message();
    return response;
  }

  DatabaseId createEntry(const CompletedBrewHistoryEntry& entry)
  {
    const auto id = history->createEmptyEntry().get();
    history->completeEntry(id, entry).get();
    return id;
  }

  BrewHistorySearchResult findHistory(const FindBrewHistory& query = {})
  {
    BrewHistorySearchResult response;
    const auto status = rpc(&OpenpressoDaemon::Stub::findBrewHistory, query, response);
    EXPECT_TRUE(status.ok()) << status.error_message();
    return response;
  }

  grpc::Status removeEntry(DatabaseId id)
  {
    PersistentId request;
    request.set_id(id);
    Empty response;
    return rpc(&OpenpressoDaemon::Stub::removeBrewHistoryEntry, request, response);
  }

  std::pair<grpc::Status, std::vector<BrewHistoryStream>> readEntry(DatabaseId id)
  {
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + 2s);
    PersistentId request;
    request.set_id(id);
    auto reader = stub->getBrewHistoryEntry(&context, request);
    std::vector<BrewHistoryStream> messages;
    BrewHistoryStream message;
    while (reader->Read(&message)) {
      messages.push_back(message);
    }
    return {reader->Finish(), std::move(messages)};
  }

  grpc::Status activateProfile(DatabaseId id)
  {
    ActivateBrewProfile request;
    request.set_persistentid(id);
    Empty response;
    return rpc(&OpenpressoDaemon::Stub::setActiveBrewProfile, request, response);
  }

  ActiveBrewProfile getActiveProfile()
  {
    ActiveBrewProfile response;
    const auto status = rpc(&OpenpressoDaemon::Stub::getActiveBrewProfile, Empty{}, response);
    EXPECT_TRUE(status.ok()) << status.error_message();
    return response;
  }

  std::shared_ptr<NiceMock<MockLibopenpressoCore>> core =
    std::make_shared<NiceMock<MockLibopenpressoCore>>();
  std::shared_ptr<NiceMock<MockBrewProfiler>> profiler = std::make_shared<NiceMock<MockBrewProfiler>>();
  std::shared_ptr<NiceMock<MockTemperatureController>> brewController =
    std::make_shared<NiceMock<MockTemperatureController>>();
  std::shared_ptr<BrewHistoryManager> historyManager;
  std::shared_ptr<AsyncEventDispatcher> dispatcher;
  std::filesystem::path directory;
  std::string profilesPath;
  std::string historyPath;
  std::shared_ptr<db::brew_profiles::AsyncBrewProfilesRepository> profiles;
  std::shared_ptr<db::brew_history::AsyncBrewHistoryRepository> history;
  std::unique_ptr<OpenpressodServiceImpl> service;
  std::unique_ptr<grpc::Server> server;
  std::unique_ptr<OpenpressoDaemon::Stub> stub;
};

TEST_F(DatabaseRpcTest, ProfileCrudPreservesCompleteDataMetadataAnd64BitIds)
{
  sql(profilesPath, "INSERT INTO sqlite_sequence(name, seq) VALUES ('profiles', 4294967295)");
  const auto profile = makeProfile("Café 'special' ☕");
  const auto before = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  const auto id = createProfile(profile);
  ASSERT_GT(id, std::numeric_limits<uint32_t>::max());
  const auto stored = getProfile(id);
  const auto after = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  EXPECT_EQ(id, stored.id());
  EXPECT_EQ(profile.SerializeAsString(), stored.profile().SerializeAsString());
  EXPECT_GE(stored.created().seconds(), before);
  EXPECT_LE(stored.created().seconds(), after);
  EXPECT_EQ(stored.created().SerializeAsString(), stored.modified().SerializeAsString());
  const auto found = findProfiles();
  ASSERT_EQ(1, found.profiles_size());
  EXPECT_EQ(stored.SerializeAsString(), found.profiles(0).SerializeAsString());

  sql(profilesPath, "UPDATE profiles SET created_at = 100, modified_at = 200");
  auto replacement = makeProfile("Updated");
  replacement.clear_stopcondition();
  replacement.mutable_steps()->SwapElements(0, 1);
  replacement.mutable_steps()->RemoveLast();
  ASSERT_TRUE(updateProfile(id, replacement).ok());
  const auto updated = getProfile(id);
  EXPECT_EQ(id, updated.id());
  EXPECT_EQ(replacement.SerializeAsString(), updated.profile().SerializeAsString());
  EXPECT_EQ(100, updated.created().seconds());
  EXPECT_GE(updated.modified().seconds(), before);
  ASSERT_TRUE(deleteProfile(id).ok());
  EXPECT_EQ(0, findProfiles().profiles_size());
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND, deleteProfile(id).error_code());
}

TEST_F(DatabaseRpcTest, MissingTargetsReturnNotFoundAndDoNotStopRepositories)
{
  PersistentId request;
  request.set_id(42);
  PersistentBrewProfile response;
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND,
            rpc(&OpenpressoDaemon::Stub::getPersistentBrewProfile, request, response).error_code());
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND, updateProfile(42, makeProfile()).error_code());
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND, deleteProfile(42).error_code());
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND, removeEntry(42).error_code());
  const auto id = createProfile(makeProfile());
  EXPECT_EQ("Espresso", getProfile(id).profile().name());
  EXPECT_EQ(0, findHistory().entries_size());
}

TEST_F(DatabaseRpcTest, ProfileSearchForwardsFiltersPaginationAndCompleteResults)
{
  EXPECT_EQ(0, findProfiles().profiles_size());
  const auto first = createProfile(makeProfile("Espresso A"));
  const auto second = createProfile(makeProfile("Other"));
  const auto third = createProfile(makeProfile("Espresso B"));
  sql(profilesPath, "UPDATE profiles SET created_at = id * 100, modified_at = id * 200");
  EXPECT_EQ((std::vector<DatabaseId>{first, second, third}), ids(findProfiles()));
  FindBrewProfiles query;
  query.set_namepattern("espresso%");
  query.mutable_mincreated()->set_seconds(100);
  query.mutable_maxcreated()->set_seconds(300);
  query.mutable_minmodified()->set_seconds(200);
  query.mutable_maxmodified()->set_seconds(600);
  EXPECT_EQ((std::vector<DatabaseId>{first, third}), ids(findProfiles(query)));
  query.set_offset(1);
  query.set_limit(1);
  const auto page = findProfiles(query);
  ASSERT_EQ(1, page.profiles_size());
  EXPECT_EQ(getProfile(third).SerializeAsString(), page.profiles(0).SerializeAsString());
  query.set_offset(0);
  query.set_limit(0);
  query.mutable_mincreated()->set_seconds(200);
  EXPECT_EQ((std::vector<DatabaseId>{third}), ids(findProfiles(query)));
  query.mutable_maxcreated()->set_seconds(200);
  EXPECT_EQ(0, findProfiles(query).profiles_size());
  query.mutable_maxcreated()->set_seconds(300);
  query.mutable_minmodified()->set_seconds(601);
  EXPECT_EQ(0, findProfiles(query).profiles_size());
  query.mutable_minmodified()->set_seconds(200);
  query.mutable_maxmodified()->set_seconds(599);
  EXPECT_EQ(0, findProfiles(query).profiles_size());
  query.Clear();
  query.set_namepattern("Missing");
  EXPECT_EQ(0, findProfiles(query).profiles_size());
  query.Clear();
  query.set_offset(3);
  EXPECT_EQ(0, findProfiles(query).profiles_size());
}

TEST_F(DatabaseRpcTest, DuplicateNamesAndProfilesReturnAlreadyExistsAndPreserveData)
{
  const auto profile = makeProfile();
  const auto id = createProfile(profile);
  const auto original = getProfile(id);
  PersistentId response;
  auto status = rpc(&OpenpressoDaemon::Stub::addPersistentBrewProfile, profile, response);
  EXPECT_EQ(grpc::StatusCode::ALREADY_EXISTS, status.error_code());
  EXPECT_EQ("Database unique constraint violated", status.error_message());
  auto sameName = profile;
  sameName.set_temperature(94'000);
  EXPECT_EQ(grpc::StatusCode::ALREADY_EXISTS,
            rpc(&OpenpressoDaemon::Stub::addPersistentBrewProfile, sameName, response).error_code());

  const auto other = createProfile(makeProfile("Other"));
  const auto otherOriginal = getProfile(other);
  EXPECT_EQ(grpc::StatusCode::ALREADY_EXISTS, updateProfile(other, sameName).error_code());
  EXPECT_EQ(grpc::StatusCode::ALREADY_EXISTS, updateProfile(other, profile).error_code());
  EXPECT_EQ(original.SerializeAsString(), getProfile(id).SerializeAsString());
  EXPECT_EQ(otherOriginal.SerializeAsString(), getProfile(other).SerializeAsString());
  EXPECT_EQ(2, findProfiles().profiles_size());
  EXPECT_TRUE(updateProfile(other, makeProfile("Replacement")).ok());
  EXPECT_GT(createProfile(makeProfile("Accepted")), other);
}

TEST_F(DatabaseRpcTest, FingerprintOnlyConflictsReturnAlreadyExists)
{
  const auto first = createProfile(makeProfile("First"));
  const auto other = createProfile(makeProfile("Other"));
  const auto candidate = makeProfile("Distinct name");
  // Isolate the fingerprint constraint without also violating the unique name constraint.
  sql(profilesPath,
      "UPDATE profiles SET fingerprint = " +
        std::to_string(static_cast<int64_t>(db::getFingerprint(candidate))) +
        " WHERE id = " + std::to_string(first));
  const auto original = getProfile(first);
  const auto otherOriginal = getProfile(other);
  PersistentId response;
  EXPECT_EQ(grpc::StatusCode::ALREADY_EXISTS,
            rpc(&OpenpressoDaemon::Stub::addPersistentBrewProfile, candidate, response).error_code());
  EXPECT_EQ(grpc::StatusCode::ALREADY_EXISTS, updateProfile(other, candidate).error_code());
  EXPECT_EQ(original.SerializeAsString(), getProfile(first).SerializeAsString());
  EXPECT_EQ(otherOriginal.SerializeAsString(), getProfile(other).SerializeAsString());
  EXPECT_TRUE(updateProfile(other, makeProfile("Accepted")).ok());
}

TEST_F(DatabaseRpcTest, CheckAndTriggerFailuresReturnInternalAndRollBackWrites)
{
  const auto id = createProfile(makeProfile());
  const auto original = getProfile(id);
  PersistentId response;
  EXPECT_EQ(grpc::StatusCode::INTERNAL,
            rpc(&OpenpressoDaemon::Stub::addPersistentBrewProfile, makeProfile(""), response).error_code());
  EXPECT_EQ(grpc::StatusCode::INTERNAL, updateProfile(id, makeProfile("")).error_code());
  EXPECT_EQ(original.SerializeAsString(), getProfile(id).SerializeAsString());
  sql(profilesPath,
      "CREATE TRIGGER fail_step BEFORE INSERT ON steps WHEN NEW.position = 1 "
      "BEGIN SELECT RAISE(ABORT, 'injected failure'); END");
  EXPECT_EQ(
    grpc::StatusCode::INTERNAL,
    rpc(&OpenpressoDaemon::Stub::addPersistentBrewProfile, makeProfile("Rejected"), response).error_code());
  EXPECT_EQ(grpc::StatusCode::INTERNAL, updateProfile(id, makeProfile("Replacement")).error_code());
  EXPECT_EQ(original.SerializeAsString(), getProfile(id).SerializeAsString());
  EXPECT_EQ(1, findProfiles().profiles_size());
  sql(profilesPath, "DROP TRIGGER fail_step");
  EXPECT_GT(createProfile(makeProfile("Accepted")), id);
  EXPECT_TRUE(updateProfile(id, makeProfile("Replacement")).ok());
}

TEST_F(DatabaseRpcTest, HistorySearchReturnsCompleteEntriesAnd64BitIds)
{
  EXPECT_EQ(0, findHistory().entries_size());
  sql(historyPath, "INSERT INTO sqlite_sequence(name, seq) VALUES ('brew_history_entries', 4294967295)");
  const auto profile = makeProfile();
  const auto entry = makeEntry(100s + 123ms, profile);
  const auto id = createEntry(entry);
  ASSERT_GT(id, std::numeric_limits<uint32_t>::max());
  const auto result = findHistory();
  ASSERT_EQ(1, result.entries_size());
  const auto& stored = result.entries(0);
  EXPECT_EQ(id, stored.id());
  EXPECT_EQ(profile.SerializeAsString(), stored.profile().SerializeAsString());
  EXPECT_EQ(100'123, TimeUtil::TimestampToMilliseconds(stored.startpoint()));
  EXPECT_EQ(10, TimeUtil::DurationToMilliseconds(stored.samplinginterval()));
  ASSERT_EQ(2, stored.samples_size());
  for (int i = 0; i < stored.samples_size(); ++i) {
    const auto& expected = entry.samples.at(static_cast<size_t>(i));
    const auto& actual = stored.samples(i);
    EXPECT_EQ(expected.pressure, actual.pressure());
    EXPECT_EQ(expected.weight, actual.weight());
    EXPECT_EQ(expected.flowRate, actual.flowrate());
    EXPECT_EQ(expected.step, actual.stepindex());
  }
  EXPECT_TRUE(removeEntry(id).ok());
  EXPECT_EQ(0, findHistory().entries_size());
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND, removeEntry(id).error_code());
}

TEST_F(DatabaseRpcTest, HistorySearchForwardsDateProfileAndPaginationFilters)
{
  const auto profile = makeProfile("Espresso");
  const auto first = createEntry(makeEntry(100s, profile));
  const auto second = createEntry(makeEntry(200s, makeProfile("Other")));
  const auto third = createEntry(makeEntry(300s, profile));
  EXPECT_EQ((std::vector<DatabaseId>{first, second, third}), ids(findHistory()));
  FindBrewHistory query;
  query.set_profilenamepattern("espresso%");
  *query.mutable_mindate() = TimeUtil::MillisecondsToTimestamp(100'000);
  *query.mutable_maxdate() = TimeUtil::MillisecondsToTimestamp(300'000);
  EXPECT_EQ((std::vector<DatabaseId>{first, third}), ids(findHistory(query)));
  query.set_offset(1);
  query.set_limit(1);
  EXPECT_EQ((std::vector<DatabaseId>{third}), ids(findHistory(query)));
  query.set_offset(0);
  query.set_limit(0);
  *query.mutable_exactprofile() = profile;
  EXPECT_EQ((std::vector<DatabaseId>{first, third}), ids(findHistory(query)));
  query.mutable_mindate()->set_seconds(101);
  EXPECT_EQ((std::vector<DatabaseId>{third}), ids(findHistory(query)));
  query.mutable_maxdate()->set_seconds(299);
  EXPECT_EQ(0, findHistory(query).entries_size());
  query.Clear();
  *query.mutable_exactprofile() = makeProfile("Missing");
  EXPECT_EQ(0, findHistory(query).entries_size());
  query.Clear();
  query.set_offset(3);
  EXPECT_EQ(0, findHistory(query).entries_size());
}

TEST_F(DatabaseRpcTest, RemovingCompletedEntriesPreservesSharedHistoryAndIncompleteReservations)
{
  const auto profile = makeProfile();
  const auto first = createEntry(makeEntry(100s + 123ms, profile));
  const auto second = createEntry(makeEntry(200s, profile));
  const auto incomplete = history->createEmptyEntry().get();
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND, removeEntry(incomplete).error_code());
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND, removeEntry(incomplete).error_code());
  EXPECT_EQ((std::vector<DatabaseId>{first, second}), ids(findHistory()));
  EXPECT_TRUE(removeEntry(first).ok());
  const auto remaining = findHistory();
  ASSERT_EQ(1, remaining.entries_size());
  EXPECT_EQ(second, remaining.entries(0).id());
  EXPECT_EQ(profile.SerializeAsString(), remaining.entries(0).profile().SerializeAsString());
  EXPECT_TRUE(removeEntry(second).ok());
  EXPECT_EQ(0, findHistory().entries_size());
  // Removing an incomplete reservation must not prevent its later completion.
  history->completeEntry(incomplete, makeEntry(300s, profile)).get();
  EXPECT_EQ((std::vector<DatabaseId>{incomplete}), ids(findHistory()));
}

TEST_F(DatabaseRpcTest, DatabaseFailuresReturnInternalForReadsSearchesAndDeletes)
{
  const auto id = createProfile(makeProfile());
  const auto entryId = createEntry(makeEntry());
  sql(profilesPath,
      "CREATE TRIGGER fail_delete BEFORE DELETE ON profiles "
      "BEGIN SELECT RAISE(ABORT, 'injected failure'); END");
  EXPECT_EQ(grpc::StatusCode::INTERNAL, deleteProfile(id).error_code());
  EXPECT_EQ(id, getProfile(id).id());
  sql(profilesPath, "DROP TRIGGER fail_delete");
  EXPECT_TRUE(deleteProfile(id).ok());

  sql(historyPath,
      "CREATE TRIGGER fail_delete BEFORE DELETE ON brew_history_entries "
      "BEGIN SELECT RAISE(ABORT, 'injected failure'); END");
  EXPECT_EQ(grpc::StatusCode::INTERNAL, removeEntry(entryId).error_code());
  EXPECT_EQ((std::vector<DatabaseId>{entryId}), ids(findHistory()));
  sql(historyPath, "DROP TRIGGER fail_delete");
  EXPECT_TRUE(removeEntry(entryId).ok());

  // Missing tables produce database errors, which must not be mistaken for missing rows.
  sql(profilesPath, "ALTER TABLE profiles RENAME TO unavailable_profiles");
  PersistentId request;
  request.set_id(id);
  PersistentBrewProfile profileResponse;
  EXPECT_EQ(grpc::StatusCode::INTERNAL,
            rpc(&OpenpressoDaemon::Stub::getPersistentBrewProfile, request, profileResponse).error_code());
  BrewProfilesSearchResult profilesResponse;
  EXPECT_EQ(
    grpc::StatusCode::INTERNAL,
    rpc(&OpenpressoDaemon::Stub::findPersistentBrewProfiles, FindBrewProfiles{}, profilesResponse).error_code());
  sql(profilesPath, "ALTER TABLE unavailable_profiles RENAME TO profiles");
  EXPECT_EQ(0, findProfiles().profiles_size());
  sql(historyPath, "ALTER TABLE brew_history_entries RENAME TO unavailable_entries");
  BrewHistorySearchResult historyResponse;
  EXPECT_EQ(grpc::StatusCode::INTERNAL,
            rpc(&OpenpressoDaemon::Stub::findBrewHistory, FindBrewHistory{}, historyResponse).error_code());
  sql(historyPath, "ALTER TABLE unavailable_entries RENAME TO brew_history_entries");
  EXPECT_EQ(0, findHistory().entries_size());
}

TEST_F(DatabaseRpcTest, GetBrewHistoryEntryByIdStreamsStoredMetadataSamplesAndStop)
{
  createEntry(makeEntry(50s, makeProfile("Other")));
  const auto profile = makeProfile("Requested profile");
  const auto entry = makeEntry(100s + 123ms, profile);
  const auto id = createEntry(entry);
  const auto [status, messages] = readEntry(id);
  ASSERT_TRUE(status.ok()) << status.error_message();
  ASSERT_EQ(entry.samples.size() + 2, messages.size());
  ASSERT_TRUE(messages.front().has_brewhistorystart());
  const auto& start = messages.front().brewhistorystart();
  EXPECT_EQ(profile.SerializeAsString(), start.profile().SerializeAsString());
  EXPECT_EQ(100'123, TimeUtil::TimestampToMilliseconds(start.startpoint()));
  EXPECT_EQ(10, TimeUtil::DurationToMilliseconds(start.samplinginterval()));
  for (size_t i = 0; i < entry.samples.size(); ++i) {
    ASSERT_TRUE(messages.at(i + 1).has_sample());
    const auto& actual = messages.at(i + 1).sample();
    const auto& expected = entry.samples.at(i);
    EXPECT_EQ(expected.pressure, actual.pressure());
    EXPECT_EQ(expected.weight, actual.weight());
    EXPECT_EQ(expected.flowRate, actual.flowrate());
    EXPECT_EQ(expected.step, actual.stepindex());
  }
  EXPECT_TRUE(messages.back().has_brewhistorystop());
}

TEST_F(DatabaseRpcTest, GetEmptyCompletedBrewHistoryEntryStreamsStartAndStop)
{
  auto entry = makeEntry();
  entry.samples.clear();
  const auto id = createEntry(entry);
  const auto [status, messages] = readEntry(id);
  ASSERT_TRUE(status.ok()) << status.error_message();
  ASSERT_EQ(2U, messages.size());
  ASSERT_TRUE(messages.front().has_brewhistorystart());
  EXPECT_EQ(entry.profile.SerializeAsString(),
            messages.front().brewhistorystart().profile().SerializeAsString());
  EXPECT_TRUE(messages.back().has_brewhistorystop());
}

TEST_F(DatabaseRpcTest, GetMissingDeletedAndIncompleteEntriesReturnsNotFoundWithoutMessages)
{
  const auto deleted = createEntry(makeEntry());
  ASSERT_TRUE(removeEntry(deleted).ok());
  const auto incomplete = history->createEmptyEntry().get();
  for (const auto id : {DatabaseId{42}, deleted, incomplete}) {
    SCOPED_TRACE(id);
    const auto [status, messages] = readEntry(id);
    EXPECT_EQ(grpc::StatusCode::NOT_FOUND, status.error_code());
    EXPECT_TRUE(messages.empty());
  }
  history->completeEntry(incomplete, makeEntry()).get();
  EXPECT_TRUE(readEntry(incomplete).first.ok());
}

TEST_F(DatabaseRpcTest, ActivatePersistentProfileAppliesStoredContentsAnd64BitId)
{
  using namespace libopenpresso::brew_step_targets;
  using namespace libopenpresso::brew_step_advance_conditions;

  sql(profilesPath, "INSERT INTO sqlite_sequence(name, seq) VALUES ('profiles', 4294967295)");
  auto profile = makeProfile("Persistent profile");
  profile.set_temperature(94'000);
  profile.mutable_stopcondition()->set_value(36'000);
  profile.mutable_steps(0)->set_targetvalue(9'000);
  profile.mutable_steps(1)->set_targetvalue(2'250);
  const auto id = createProfile(profile);
  ASSERT_GT(id, std::numeric_limits<uint32_t>::max());
  const auto original = getProfile(id);
  EXPECT_CALL(*brewController, setTargetTemperature(94'000));
  // Converting float seconds to nanoseconds can introduce sub-microsecond rounding.
  const auto stepTime = testing::AllOf(testing::Ge(5500ms - 1us), testing::Le(5500ms + 1us));
  EXPECT_CALL(
    *profiler,
    setSteps(testing::ElementsAre(
      testing::Pair(testing::VariantWith<ConstantPressure>(testing::Field(&ConstantPressure::pressure, 9'000)),
                    testing::VariantWith<OnStepTime>(testing::Field(&OnStepTime::time, stepTime))),
      testing::Pair(testing::VariantWith<ConstantFlow>(testing::Field(&ConstantFlow::rate, 2'250)),
                    testing::VariantWith<Never>(_)))));
  EXPECT_CALL(*profiler,
              setAutoStopCondition(testing::Matcher<OnWeight>(testing::Field(&OnWeight::weight, 36'000))));
  const auto status = activateProfile(id);
  ASSERT_TRUE(status.ok()) << status.error_message();
  const auto active = getActiveProfile();
  ASSERT_TRUE(active.has_profile());
  EXPECT_EQ(profile.SerializeAsString(), active.profile().SerializeAsString());
  EXPECT_EQ(original.SerializeAsString(), getProfile(id).SerializeAsString());
  auto config =
    OpenpressodConfig::fromString("[global]\ndaemon_data_directory = '" + directory.string() + "'\n");
  BrewProfileManager saved(config);
  saved.loadSavedProfile();
  EXPECT_EQ(profile.SerializeAsString(), saved.getProfile().SerializeAsString());
}

TEST_F(DatabaseRpcTest, ActivatePersistentProfileLoadsLatestDatabaseVersion)
{
  const auto profile = makeProfile("Original");
  const auto id = createProfile(profile);
  ASSERT_TRUE(activateProfile(id).ok());
  auto replacement = makeProfile("Updated in database");
  replacement.set_temperature(95'000);
  replacement.clear_stopcondition();
  replacement.mutable_steps()->SwapElements(0, 1);
  ASSERT_TRUE(updateProfile(id, replacement).ok());
  EXPECT_EQ(profile.SerializeAsString(), getActiveProfile().profile().SerializeAsString());
  ASSERT_TRUE(activateProfile(id).ok());
  EXPECT_EQ(replacement.SerializeAsString(), getActiveProfile().profile().SerializeAsString());
}

TEST_F(DatabaseRpcTest, MissingPersistentProfileAndEmptyActivationLeaveActiveProfileUnchanged)
{
  const auto initial = getActiveProfile();
  EXPECT_CALL(*brewController, setTargetTemperature(_)).Times(0);
  EXPECT_CALL(*profiler, setSteps(_)).Times(0);
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND, activateProfile(42).error_code());
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND, activateProfile(0).error_code());
  Empty response;
  EXPECT_EQ(grpc::StatusCode::INVALID_ARGUMENT,
            rpc(&OpenpressoDaemon::Stub::setActiveBrewProfile, ActivateBrewProfile{}, response).error_code());
  EXPECT_EQ(initial.SerializeAsString(), getActiveProfile().SerializeAsString());
}

TEST_F(DatabaseRpcTest, PersistentActivationDatabaseFailureReturnsInternalAndCanBeRetried)
{
  const auto id = createProfile(makeProfile("Stored"));
  const auto initial = getActiveProfile();
  sql(profilesPath, "ALTER TABLE profiles RENAME TO unavailable_profiles");
  EXPECT_EQ(grpc::StatusCode::INTERNAL, activateProfile(id).error_code());
  EXPECT_EQ(initial.SerializeAsString(), getActiveProfile().SerializeAsString());
  sql(profilesPath, "ALTER TABLE unavailable_profiles RENAME TO profiles");
  ASSERT_TRUE(activateProfile(id).ok());
  EXPECT_EQ("Stored", getActiveProfile().profile().name());
}

TEST_F(DatabaseRpcTest, PersistentActivationWhileBrewingIsAbortedAndCanBeRetried)
{
  const auto id = createProfile(makeProfile("Stored"));
  const auto initial = getActiveProfile();
  ON_CALL(*profiler, isActive()).WillByDefault(Return(true));
  EXPECT_CALL(*brewController, setTargetTemperature(_)).Times(0);
  EXPECT_CALL(*profiler, setSteps(_)).Times(0);
  const auto status = activateProfile(id);
  ON_CALL(*profiler, isActive()).WillByDefault(Return(false));
  EXPECT_EQ(grpc::StatusCode::ABORTED, status.error_code());
  EXPECT_EQ(initial.SerializeAsString(), getActiveProfile().SerializeAsString());
  testing::Mock::VerifyAndClearExpectations(brewController.get());
  testing::Mock::VerifyAndClearExpectations(profiler.get());
  ASSERT_TRUE(activateProfile(id).ok());
  EXPECT_EQ("Stored", getActiveProfile().profile().name());
}

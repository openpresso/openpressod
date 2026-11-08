#include "brew_log/brew_log_data.hpp"
#include "brew_log/brew_log_manager.hpp"
#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"
#include "database/brew_history/async_repository.hpp"
#include "libopenpresso_core_mock.hpp"
#include "pressure_sensor_mock.hpp"
#include "service/openppressod_service_impl.hpp"
#include "weight_sensor_mock.hpp"

#include <chrono>
#include <cstddef>
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
#include <openpresso_proto/openpresso.grpc.pb.h>

using namespace openpressod;
using namespace std::chrono_literals;
using google::protobuf::util::TimeUtil;
using testing::NiceMock;
using testing::Return;

namespace
{

class BrewLogManagerTest : public testing::Test {
protected:
  struct Stream {
    grpc::ClientContext context;
    std::unique_ptr<grpc::ClientReader<BrewLogStream>> reader;
    bool finished = false;

    Stream(OpenpressoDaemon::Stub& stub, uint64_t brewId)
    {
      context.set_deadline(std::chrono::system_clock::now() + 5s);
      BrewLogId request;
      request.set_id(brewId);
      reader = stub.getBrewLog(&context, request);
    }

    ~Stream()
    {
      if (!finished) {
        context.TryCancel();
        reader->Finish();
      }
    }

    bool next(BrewLogStream& message)
    {
      return reader->Read(&message);
    }

    grpc::Status finish()
    {
      finished = true;
      return reader->Finish();
    }
  };

  static BrewProfile makeProfile(std::string name = "Espresso")
  {
    BrewProfile profile;
    profile.set_name(std::move(name));
    profile.set_temperature(93'000);
    profile.mutable_stopcondition()->set_type(StopConditionType_TotalWeight);
    profile.mutable_stopcondition()->set_value(36.5F);
    auto* step = profile.add_steps();
    step->set_targettype(BrewStepTargetType_Pressure);
    step->set_targetvalue(9.0F);
    return profile;
  }

  void SetUp() override
  {
    const auto base = std::filesystem::temp_directory_path() /
                      ("openpressod-brew-log-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    directory = base;
    for (unsigned suffix = 0; !std::filesystem::create_directory(directory); ++suffix) {
      directory = base.string() + "-" + std::to_string(suffix);
    }
    path = (directory / "history.sqlite").string();
    history = std::make_shared<db::brew_history::AsyncBrewHistoryRepository>(path);
    ON_CALL(*core, getPressureSensor(libopenpresso_config_labels::PRESSURE_SENSOR_LABEL))
      .WillByDefault(Return(pressure));
    ON_CALL(*core, getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL)).WillByDefault(Return(weight));
    ON_CALL(*pressure, getPressure()).WillByDefault(Return(8'500));
    ON_CALL(*weight, getWeight()).WillByDefault(Return(-200));
    ON_CALL(*weight, getFlowRate()).WillByDefault(Return(-300));
  }

  void startServer(const std::string& interval = "00:00:00.020")
  {
    const auto config =
      OpenpressodConfig::fromString("[global]\nbrew_log_sampling_interval = " + interval);
    manager = std::make_shared<BrewLogManager>(core, history, config);
    service =
      std::make_unique<OpenpressodServiceImpl>(nullptr, manager, nullptr, nullptr, nullptr, history);
    grpc::ServerBuilder builder;
    builder.RegisterService(service.get());
    server = builder.BuildAndStart();
    ASSERT_NE(server, nullptr);
    stub = OpenpressoDaemon::NewStub(server->InProcessChannel(grpc::ChannelArguments{}));
  }

  void shutdown()
  {
    streams.clear();
    stub.reset();
    if (server) {
      server->Shutdown(std::chrono::system_clock::now() + 2s);
      server->Wait();
    }
    server.reset();
    service.reset();
    manager.reset();
  }

  void TearDown() override
  {
    shutdown();
    history.reset();
    std::error_code error;
    std::filesystem::remove_all(directory, error);
  }

  Stream& subscribe(uint64_t brewId)
  {
    streams.push_back(std::make_unique<Stream>(*stub, brewId));
    return *streams.back();
  }

  std::vector<BrewLogStream> readToEnd(Stream& stream)
  {
    std::vector<BrewLogStream> messages;
    BrewLogStream message;
    while (stream.next(message)) {
      messages.push_back(message);
    }
    const auto status = stream.finish();
    EXPECT_TRUE(status.ok()) << status.error_message();
    return messages;
  }

  static void expectRecord(const BrewLogStream& message, size_t step = 0)
  {
    ASSERT_TRUE(message.has_record());
    EXPECT_EQ(8'500U, message.record().pressure());
    EXPECT_EQ(-200, message.record().weight());
    EXPECT_EQ(-300, message.record().flowrate());
    EXPECT_EQ(step, message.record().stepindex());
  }

  static void expectCompleted(const std::vector<BrewLogStream>& messages)
  {
    ASSERT_GE(messages.size(), 3U);
    EXPECT_TRUE(messages.front().has_brewlogstart());
    EXPECT_TRUE(messages.back().has_brewlogstop());
    for (size_t index = 1; index + 1 < messages.size(); ++index) {
      EXPECT_TRUE(messages[index].has_record());
    }
  }

  std::filesystem::path directory;
  std::string path;
  std::shared_ptr<NiceMock<MockLibopenpressoCore>> core =
    std::make_shared<NiceMock<MockLibopenpressoCore>>();
  std::shared_ptr<NiceMock<MockPressureSensor>> pressure =
    std::make_shared<NiceMock<MockPressureSensor>>();
  std::shared_ptr<NiceMock<MockWeightSensor>> weight = std::make_shared<NiceMock<MockWeightSensor>>();
  std::shared_ptr<db::brew_history::AsyncBrewHistoryRepository> history;
  std::shared_ptr<BrewLogManager> manager;
  std::unique_ptr<OpenpressodServiceImpl> service;
  std::unique_ptr<grpc::Server> server;
  std::unique_ptr<OpenpressoDaemon::Stub> stub;
  std::vector<std::unique_ptr<Stream>> streams;
};

TEST_F(BrewLogManagerTest, StreamsStartMetadataProfileAndLiveRecordsWithStepChanges)
{
  startServer();
  auto profile = makeProfile();
  const auto expectedProfile = profile.SerializeAsString();
  const auto id = history->createEmptyLog().get();
  const auto before = std::chrono::system_clock::now();
  manager->startBrew(id, profile);
  profile.set_name("Edited after starting");
  auto& stream = subscribe(id);
  BrewLogStream message;
  ASSERT_TRUE(stream.next(message));
  ASSERT_TRUE(message.has_brewlogstart());
  const auto& start = message.brewlogstart();
  EXPECT_EQ(20'000'000, TimeUtil::DurationToNanoseconds(start.samplinginterval()));
  const auto started = std::chrono::nanoseconds{TimeUtil::TimestampToNanoseconds(start.startpoint())};
  EXPECT_GE(started, before.time_since_epoch());
  EXPECT_LE(started, std::chrono::system_clock::now().time_since_epoch());
  EXPECT_EQ(expectedProfile, start.profile().SerializeAsString());
  ASSERT_TRUE(stream.next(message));
  expectRecord(message);

  manager->setCurrentStep(3);
  do {
    ASSERT_TRUE(stream.next(message));
    ASSERT_TRUE(message.has_record());
  } while (message.record().stepindex() != 3);
  expectRecord(message, 3);
  manager->stopBrew();
  const auto tail = readToEnd(stream);
  ASSERT_FALSE(tail.empty());
  EXPECT_TRUE(tail.back().has_brewlogstop());
  for (size_t index = 0; index + 1 < tail.size(); ++index) {
    expectRecord(tail[index], 3);
  }
}

TEST_F(BrewLogManagerTest, LateSubscriberReceivesTheSameBufferedPrefixThenLiveRecords)
{
  startServer();
  const auto id = history->createEmptyLog().get();
  manager->startBrew(id, makeProfile());
  auto& first = subscribe(id);
  std::vector<BrewLogStream> prefix(4);
  for (auto& message : prefix) {
    ASSERT_TRUE(first.next(message));
  }
  auto& late = subscribe(id);
  BrewLogStream message;
  for (const auto& expected : prefix) {
    ASSERT_TRUE(late.next(message));
    EXPECT_EQ(expected.SerializeAsString(), message.SerializeAsString());
  }
  manager->setCurrentStep(2);
  do {
    ASSERT_TRUE(late.next(message));
    ASSERT_TRUE(message.has_record());
  } while (message.record().stepindex() != 2);
  manager->stopBrew();
  const auto firstTail = readToEnd(first);
  const auto lateTail = readToEnd(late);
  ASSERT_FALSE(firstTail.empty());
  ASSERT_FALSE(lateTail.empty());
  EXPECT_TRUE(firstTail.back().has_brewlogstop());
  EXPECT_TRUE(lateTail.back().has_brewlogstop());
}

TEST_F(BrewLogManagerTest, StopPersistsExactlyTheRecordsDeliveredToLiveSubscribers)
{
  startServer();
  const auto id = history->createEmptyLog().get();
  const auto profile = makeProfile();
  manager->startBrew(id, profile);
  auto& live = subscribe(id);
  std::vector<BrewLogStream> messages(3);
  for (auto& message : messages) {
    ASSERT_TRUE(live.next(message));
  }
  manager->setCurrentStep(4);
  BrewLogStream message;
  do {
    ASSERT_TRUE(live.next(message));
    ASSERT_TRUE(message.has_record());
    messages.push_back(message);
  } while (message.record().stepindex() != 4);
  manager->stopBrew();
  auto tail = readToEnd(live);
  messages.insert(messages.end(), tail.begin(), tail.end());
  expectCompleted(messages);

  // Subscribing is queued after stop, so the history lookup must observe the completed write.
  const auto replay = readToEnd(subscribe(id));
  ASSERT_EQ(messages.size(), replay.size());
  expectCompleted(replay);
  EXPECT_EQ(profile.SerializeAsString(), replay.front().brewlogstart().profile().SerializeAsString());
  EXPECT_EQ(TimeUtil::TimestampToMilliseconds(messages.front().brewlogstart().startpoint()),
            TimeUtil::TimestampToMilliseconds(replay.front().brewlogstart().startpoint()));
  EXPECT_EQ(messages.front().brewlogstart().samplinginterval().SerializeAsString(),
            replay.front().brewlogstart().samplinginterval().SerializeAsString());
  for (size_t index = 1; index < messages.size(); ++index) {
    EXPECT_EQ(messages[index].SerializeAsString(), replay[index].SerializeAsString());
  }
}

TEST_F(BrewLogManagerTest, CancellingOneActiveSubscriberLeavesOtherSubscribersAndSamplingRunning)
{
  startServer();
  const auto id = history->createEmptyLog().get();
  manager->startBrew(id, makeProfile());
  auto& cancelled = subscribe(id);
  auto& live = subscribe(id);
  BrewLogStream message;
  ASSERT_TRUE(cancelled.next(message));
  ASSERT_TRUE(cancelled.next(message));
  ASSERT_TRUE(live.next(message));
  ASSERT_TRUE(live.next(message));
  cancelled.context.TryCancel();
  EXPECT_EQ(grpc::StatusCode::CANCELLED, cancelled.finish().error_code());
  manager->setCurrentStep(5);
  do {
    ASSERT_TRUE(live.next(message));
    ASSERT_TRUE(message.has_record());
  } while (message.record().stepindex() != 5);
  manager->stopBrew();
  const auto tail = readToEnd(live);
  ASSERT_FALSE(tail.empty());
  EXPECT_TRUE(tail.back().has_brewlogstop());
}

TEST_F(BrewLogManagerTest, ConsecutiveBrewsResetStepRecordsAndProfileWithoutOverwritingEarlierLogs)
{
  startServer("01:00:00");
  std::vector<uint64_t> ids;
  for (const auto* name : {"First", "Second"}) {
    const auto id = history->createEmptyLog().get();
    ids.push_back(id);
    manager->setCurrentStep(99);
    manager->startBrew(id, makeProfile(name));
    manager->stopBrew();
    const auto replay = readToEnd(subscribe(id));
    ASSERT_EQ(3U, replay.size());
    expectCompleted(replay);
    expectRecord(replay[1]);
    EXPECT_EQ(name, replay.front().brewlogstart().profile().name());
  }
  const auto first = readToEnd(subscribe(ids.front()));
  ASSERT_EQ(3U, first.size());
  EXPECT_EQ("First", first.front().brewlogstart().profile().name());
}

TEST_F(BrewLogManagerTest, MissingLogAndCancellingTheLastSubscriberDoNotInterruptTheActiveBrew)
{
  startServer();
  const auto id = history->createEmptyLog().get();
  manager->startBrew(id, makeProfile());
  BrewLogStream message;
  auto& missing = subscribe(id + 1);
  EXPECT_FALSE(missing.next(message));
  EXPECT_EQ(grpc::StatusCode::NOT_FOUND, missing.finish().error_code());

  auto& first = subscribe(id);
  ASSERT_TRUE(first.next(message));
  const auto start = message.SerializeAsString();
  ASSERT_TRUE(first.next(message));
  const auto firstRecord = message.SerializeAsString();
  first.context.TryCancel();
  EXPECT_EQ(grpc::StatusCode::CANCELLED, first.finish().error_code());

  manager->setCurrentStep(7);
  auto& reconnected = subscribe(id);
  ASSERT_TRUE(reconnected.next(message));
  EXPECT_EQ(start, message.SerializeAsString());
  ASSERT_TRUE(reconnected.next(message));
  EXPECT_EQ(firstRecord, message.SerializeAsString());
  do {
    ASSERT_TRUE(reconnected.next(message));
    ASSERT_TRUE(message.has_record());
  } while (message.record().stepindex() != 7);
  manager->stopBrew();
  const auto tail = readToEnd(reconnected);
  ASSERT_FALSE(tail.empty());
  EXPECT_TRUE(tail.back().has_brewlogstop());
}

TEST_F(BrewLogManagerTest, DestructionStopsAnActiveSamplerWithoutWaitingForTheNextTick)
{
  startServer("01:00:00");
  const auto id = history->createEmptyLog().get();
  manager->startBrew(id, makeProfile());
  auto& stream = subscribe(id);
  BrewLogStream message;
  ASSERT_TRUE(stream.next(message));
  ASSERT_TRUE(stream.next(message));
  expectRecord(message);
  // Exercise destruction while sampling, with no explicit stopBrew call.
  const auto before = std::chrono::steady_clock::now();
  shutdown();
  EXPECT_LT(std::chrono::steady_clock::now() - before, 2s);
}

TEST_F(BrewLogManagerTest, CompletedHistoryCanBeReadWhileAnotherBrewIsActive)
{
  startServer("01:00:00");
  const auto completedId = history->createEmptyLog().get();
  manager->startBrew(completedId, makeProfile("Completed"));
  manager->stopBrew();
  const auto activeId = history->createEmptyLog().get();
  manager->startBrew(activeId, makeProfile("Active"));
  const auto completed = readToEnd(subscribe(completedId));
  ASSERT_EQ(3U, completed.size());
  EXPECT_EQ("Completed", completed.front().brewlogstart().profile().name());
  auto& active = subscribe(activeId);
  BrewLogStream message;
  ASSERT_TRUE(active.next(message));
  ASSERT_TRUE(message.has_brewlogstart());
  EXPECT_EQ("Active", message.brewlogstart().profile().name());
  ASSERT_TRUE(active.next(message));
  expectRecord(message);
  manager->stopBrew();
  ASSERT_TRUE(active.next(message));
  EXPECT_TRUE(message.has_brewlogstop());
  EXPECT_TRUE(readToEnd(active).empty());
}

TEST_F(BrewLogManagerTest, RepeatedStopAndIdleStopDoNotCreateOrModifyHistory)
{
  startServer("01:00:00");
  manager->stopBrew();
  manager->stopBrew();
  const auto id = history->createEmptyLog().get();
  manager->startBrew(id, makeProfile());
  manager->stopBrew();
  manager->stopBrew();
  const auto replay = readToEnd(subscribe(id));
  ASSERT_EQ(3U, replay.size());
  shutdown();
  const auto result = history->find({}).get();
  ASSERT_EQ(1, result.logs_size());
  EXPECT_EQ(id, result.logs(0).id());
  EXPECT_EQ(1, result.logs(0).records_size());
}

TEST_F(BrewLogManagerTest, BrewWithoutSubscribersIsPersistedAndSurvivesRepositoryReopening)
{
  startServer("01:00:00");
  const auto id = history->createEmptyLog().get();
  const auto profile = makeProfile();
  manager->startBrew(id, profile);
  manager->stopBrew();
  // Destruction drains the manager queue; reopening must wait for the repository queue as well.
  shutdown();
  history.reset();
  history = std::make_shared<db::brew_history::AsyncBrewHistoryRepository>(path);
  const auto [log, storedProfile] = history->get(id).get();
  EXPECT_EQ(profile.SerializeAsString(), storedProfile.SerializeAsString());
  EXPECT_EQ(1h, log.samplingInterval);
  ASSERT_EQ(1U, log.records.size());
  EXPECT_EQ(8'500U, log.records.front().pressure);
  EXPECT_EQ(-200, log.records.front().weight);
  EXPECT_EQ(-300, log.records.front().flowRate);
  EXPECT_EQ(0U, log.records.front().step);
  startServer();
  const auto replay = readToEnd(subscribe(id));
  ASSERT_EQ(3U, replay.size());
  expectCompleted(replay);
  EXPECT_EQ(TimeUtil::NanosecondsToDuration(3'600'000'000'000).SerializeAsString(),
            replay.front().brewlogstart().samplinginterval().SerializeAsString());
}

TEST_F(BrewLogManagerTest, ActiveAndCompletedLogsKeepRepositoryIdsAboveUint32Range)
{
  sqlite3* connection = nullptr;
  ASSERT_EQ(SQLITE_OK, sqlite3_open(path.c_str(), &connection));
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(connection, sqlite3_close);
  ASSERT_EQ(SQLITE_OK,
            sqlite3_exec(database.get(),
                         "INSERT INTO sqlite_sequence(name, seq) VALUES ('brew_logs', 4294967295)",
                         nullptr,
                         nullptr,
                         nullptr));
  const auto id = history->createEmptyLog().get();
  ASSERT_GT(id, std::numeric_limits<uint32_t>::max());
  startServer("01:00:00");
  manager->startBrew(id, makeProfile());
  auto& live = subscribe(id);
  BrewLogStream message;
  ASSERT_TRUE(live.next(message));
  EXPECT_TRUE(message.has_brewlogstart());
  ASSERT_TRUE(live.next(message));
  expectRecord(message);
  manager->stopBrew();
  ASSERT_TRUE(live.next(message));
  EXPECT_TRUE(message.has_brewlogstop());
  EXPECT_TRUE(readToEnd(live).empty());
  const auto replay = readToEnd(subscribe(id));
  ASSERT_EQ(3U, replay.size());
  expectCompleted(replay);
}

} // namespace

#include "async_events_dispatcher/async_events_dispatcher.hpp"
#include "brew_log/brew_log_manager.hpp"
#include "brew_log/brew_log_sampler.hpp"
#include "brew_profiler_mock.hpp"
#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"
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
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <gmock/gmock.h>
#include <google/protobuf/empty.pb.h>
#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>
#include <openpresso_proto/openpresso.grpc.pb.h>
#include <openpresso_proto/openpresso.pb.h>

using namespace openpressod;
using namespace testing;
using namespace std::chrono_literals;

namespace
{

TEST(BrewLogSampler, SamplesAllMetricsAndCanStopAndRestart)
{
  auto core = std::make_shared<NiceMock<MockLibopenpressoCore>>();
  auto temperature = std::make_shared<NiceMock<MockTemperatureSensor>>();
  auto pressure = std::make_shared<NiceMock<MockPressureSensor>>();
  auto weight = std::make_shared<NiceMock<MockWeightSensor>>();
  ON_CALL(*core, getTemperatureSensor(libopenpresso_config_labels::TEMPERATURE_SENSOR_LABEL))
    .WillByDefault(Return(temperature));
  ON_CALL(*core, getPressureSensor(libopenpresso_config_labels::PRESSURE_SENSOR_LABEL))
    .WillByDefault(Return(pressure));
  ON_CALL(*core, getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL)).WillByDefault(Return(weight));
  ON_CALL(*temperature, getTemperature()).WillByDefault(Return(93'000));
  ON_CALL(*pressure, getPressure()).WillByDefault(Return(8'500));
  ON_CALL(*weight, getWeight()).WillByDefault(Return(17'000));
  ON_CALL(*weight, getFlowRate()).WillByDefault(Return(2'100));

  BrewLogSampler sampler(core, 10ms);
  std::mutex mutex;
  std::condition_variable sampled;
  using Sample = std::tuple<int, int, int, int, size_t>;
  std::vector<Sample> samples;
  auto capture = [&](auto t, auto p, auto w, auto f, auto step) {
    {
      std::scoped_lock lock(mutex);
      samples.emplace_back(t, p, w, f, step);
    }
    sampled.notify_one();
  };
  auto waitFor = [&](size_t count) {
    std::unique_lock lock(mutex);
    return sampled.wait_for(lock, 1s, [&] { return samples.size() >= count; });
  };

  sampler.startSampling(capture);
  EXPECT_TRUE(waitFor(2));
  sampler.stopSampling();
  size_t stoppedCount;
  {
    std::scoped_lock lock(mutex);
    stoppedCount = samples.size();
    EXPECT_THAT(samples, Each(Sample{93'000, 8'500, 17'000, 2'100, 0}));
  }
  std::this_thread::sleep_for(30ms);
  {
    std::scoped_lock lock(mutex);
    EXPECT_EQ(stoppedCount, samples.size());
  }

  sampler.setCurrentStep(2);
  sampler.startSampling(capture);
  EXPECT_TRUE(waitFor(stoppedCount + 1));
  sampler.stopSampling();
  sampler.stopSampling();
  EXPECT_EQ((Sample{93'000, 8'500, 17'000, 2'100, 0}), samples.back());
}

class BrewLogRpcTest : public Test {
protected:
  struct Stream {
    grpc::ClientContext context;
    std::unique_ptr<grpc::ClientReader<BrewLog>> reader;
    bool finished = false;

    Stream(OpenpressoDaemon::Stub& stub, uint32_t brewId, std::chrono::milliseconds deadline = 2s)
    {
      context.set_deadline(std::chrono::system_clock::now() + deadline);
      BrewLogRequest request;
      request.set_brewid(brewId);
      reader = stub.brewLog(&context, request);
    }

    ~Stream()
    {
      if (!finished) {
        context.TryCancel();
        reader->Finish();
      }
    }

    bool next(BrewLog& record)
    {
      return reader->Read(&record);
    }

    grpc::Status finish()
    {
      finished = true;
      return reader->Finish();
    }
  };

  void startServer(const std::string& interval = "00:00:05", size_t retainedLogs = 10)
  {
    ON_CALL(*core, getTemperatureSensor(libopenpresso_config_labels::TEMPERATURE_SENSOR_LABEL))
      .WillByDefault(Return(temperature));
    ON_CALL(*core, getPressureSensor(libopenpresso_config_labels::PRESSURE_SENSOR_LABEL))
      .WillByDefault(Return(pressure));
    ON_CALL(*core, getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL)).WillByDefault(Return(weight));
    ON_CALL(*temperature, getTemperature()).WillByDefault(Return(93'000));
    ON_CALL(*pressure, getPressure()).WillByDefault(Return(8'500));
    ON_CALL(*weight, getWeight()).WillByDefault(Return(17'000));
    ON_CALL(*weight, getFlowRate()).WillByDefault(Return(2'100));

    auto config =
      OpenpressodConfig::fromString("[global]\nbrew_log_sampling_interval = " + interval +
                                    "\nbrew_logs_retantion_count = " + std::to_string(retainedLogs));
    manager = std::make_shared<BrewLogManager>(core, config);
    service = std::make_unique<OpenpressodServiceImpl>(nullptr, manager, nullptr, nullptr);
    grpc::ServerBuilder builder;
    int port = 0;
    builder.AddListeningPort("127.0.0.1:0", grpc::InsecureServerCredentials(), &port);
    builder.RegisterService(service.get());
    server = builder.BuildAndStart();
    ASSERT_NE(server, nullptr);
    stub = OpenpressoDaemon::NewStub(grpc::CreateChannel("127.0.0.1:" + std::to_string(port),
                                                         grpc::InsecureChannelCredentials()));
  }

  void TearDown() override
  {
    streams.clear();
    if (server) {
      server->Shutdown();
    }
    server.reset();
    service.reset();
    manager.reset();
  }

  Stream& subscribe(uint32_t brewId, std::chrono::milliseconds deadline = 2s)
  {
    streams.push_back(std::make_unique<Stream>(*stub, brewId, deadline));
    return *streams.back();
  }

  std::shared_ptr<NiceMock<MockLibopenpressoCore>> core =
    std::make_shared<NiceMock<MockLibopenpressoCore>>();
  std::shared_ptr<NiceMock<MockTemperatureSensor>> temperature =
    std::make_shared<NiceMock<MockTemperatureSensor>>();
  std::shared_ptr<NiceMock<MockPressureSensor>> pressure =
    std::make_shared<NiceMock<MockPressureSensor>>();
  std::shared_ptr<NiceMock<MockWeightSensor>> weight = std::make_shared<NiceMock<MockWeightSensor>>();
  std::shared_ptr<BrewLogManager> manager;
  std::unique_ptr<OpenpressodServiceImpl> service;
  std::unique_ptr<grpc::Server> server;
  std::unique_ptr<OpenpressoDaemon::Stub> stub;
  std::vector<std::unique_ptr<Stream>> streams;
};

TEST_F(BrewLogRpcTest, StartRecordsWithStepIdsAndStopAreStreamedInOrder)
{
  startServer("00:00:00.050");
  const auto before = std::chrono::system_clock::now();
  manager->startBrew(1);
  auto& stream = subscribe(1);
  BrewLog record;
  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(record.has_brewlogstart());
  EXPECT_EQ(0, record.brewlogstart().samplinginterval().seconds());
  EXPECT_EQ(50'000'000, record.brewlogstart().samplinginterval().nanos());
  const auto started = std::chrono::seconds(record.brewlogstart().startpoint().seconds()) +
                       std::chrono::nanoseconds(record.brewlogstart().startpoint().nanos());
  EXPECT_GE(started, before.time_since_epoch());
  EXPECT_LE(started, std::chrono::system_clock::now().time_since_epoch());

  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(record.has_record());
  EXPECT_EQ(93'000, record.record().temperature());
  EXPECT_EQ(8'500U, record.record().pressure());
  EXPECT_EQ(17'000, record.record().weight());
  EXPECT_EQ(2'100, record.record().flowrate());
  EXPECT_EQ(0U, record.record().stepindex());

  manager->setCurrentStep(2);
  do {
    ASSERT_TRUE(stream.next(record));
    ASSERT_TRUE(record.has_record());
  } while (record.record().stepindex() != 2);
  EXPECT_EQ(93'000, record.record().temperature());
  EXPECT_EQ(2U, record.record().stepindex());
  manager->stopBrew();
  do {
    ASSERT_TRUE(stream.next(record));
  } while (record.has_record());
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_Ok, record.brewlogstop().result());
  EXPECT_FALSE(stream.next(record));
  EXPECT_TRUE(stream.finish().ok());

  auto& replay = subscribe(1);
  ASSERT_TRUE(replay.next(record));
  ASSERT_TRUE(record.has_brewlogstart());
  bool replayedStepTwo = false;
  while (replay.next(record) && record.has_record()) {
    EXPECT_THAT(record.record().stepindex(), AnyOf(0U, 2U));
    replayedStepTwo |= record.record().stepindex() == 2;
  }
  EXPECT_TRUE(replayedStepTwo);
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_Ok, record.brewlogstop().result());
  EXPECT_FALSE(replay.next(record));
  EXPECT_TRUE(replay.finish().ok());
}

TEST_F(BrewLogRpcTest, LateSubscribersGetCurrentAndCompletedLog)
{
  startServer();
  manager->startBrew(1);
  auto& first = subscribe(1);
  BrewLog record;
  ASSERT_TRUE(first.next(record));
  ASSERT_TRUE(first.next(record));
  ASSERT_TRUE(record.has_record());

  auto& during = subscribe(1);
  ASSERT_TRUE(during.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(during.next(record));
  EXPECT_TRUE(record.has_record());

  manager->stopBrew();
  ASSERT_TRUE(during.next(record));
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_Ok, record.brewlogstop().result());
  EXPECT_FALSE(during.next(record));
  EXPECT_TRUE(during.finish().ok());
  ASSERT_TRUE(first.next(record));
  EXPECT_TRUE(record.has_brewlogstop());
  EXPECT_FALSE(first.next(record));
  EXPECT_TRUE(first.finish().ok());

  auto& after = subscribe(1);
  ASSERT_TRUE(after.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(after.next(record));
  EXPECT_TRUE(record.has_record());
  ASSERT_TRUE(after.next(record));
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_Ok, record.brewlogstop().result());
  EXPECT_FALSE(after.next(record));
  EXPECT_TRUE(after.finish().ok());
}

TEST_F(BrewLogRpcTest, RetainsConfiguredNumberOfCompletedLogs)
{
  startServer("00:00:05", 2);
  for (uint32_t brewId = 1; brewId <= 3; ++brewId) {
    manager->startBrew(brewId);
    manager->stopBrew();
  }

  BrewLog record;
  auto& evicted = subscribe(1);
  ASSERT_TRUE(evicted.next(record));
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_IdNotFound, record.brewlogstop().result());
  EXPECT_FALSE(evicted.next(record));
  EXPECT_TRUE(evicted.finish().ok());

  for (uint32_t brewId = 2; brewId <= 3; ++brewId) {
    auto& retained = subscribe(brewId);
    ASSERT_TRUE(retained.next(record));
    EXPECT_TRUE(record.has_brewlogstart());
    ASSERT_TRUE(retained.next(record));
    EXPECT_TRUE(record.has_record());
    ASSERT_TRUE(retained.next(record));
    ASSERT_TRUE(record.has_brewlogstop());
    EXPECT_EQ(BrewLogResult_Ok, record.brewlogstop().result());
    EXPECT_FALSE(retained.next(record));
    EXPECT_TRUE(retained.finish().ok());
  }
}

TEST_F(BrewLogRpcTest, ActiveBrewDoesNotConsumeCompletedRetentionSlot)
{
  startServer("00:00:05", 1);
  manager->startBrew(1);
  manager->stopBrew();
  manager->startBrew(2);

  BrewLog record;
  auto& completed = subscribe(1);
  ASSERT_TRUE(completed.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(completed.next(record));
  EXPECT_TRUE(record.has_record());
  ASSERT_TRUE(completed.next(record));
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_Ok, record.brewlogstop().result());
  EXPECT_FALSE(completed.next(record));
  EXPECT_TRUE(completed.finish().ok());

  auto& active = subscribe(2);
  ASSERT_TRUE(active.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(active.next(record));
  EXPECT_TRUE(record.has_record());

  manager->stopBrew();
  ASSERT_TRUE(active.next(record));
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_Ok, record.brewlogstop().result());
  EXPECT_FALSE(active.next(record));
  EXPECT_TRUE(active.finish().ok());

  auto& evicted = subscribe(1);
  ASSERT_TRUE(evicted.next(record));
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_IdNotFound, record.brewlogstop().result());
  EXPECT_FALSE(evicted.next(record));
  EXPECT_TRUE(evicted.finish().ok());
}

TEST_F(BrewLogRpcTest, CancellingOneSubscriberDoesNotStopAnother)
{
  startServer();
  manager->startBrew(1);
  auto& cancelled = subscribe(1);
  auto& live = subscribe(1);
  cancelled.context.TryCancel();
  BrewLog record;
  ASSERT_TRUE(live.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(live.next(record));
  EXPECT_TRUE(record.has_record());
  manager->stopBrew();
  ASSERT_TRUE(live.next(record));
  EXPECT_TRUE(record.has_brewlogstop());
  EXPECT_FALSE(live.next(record));
  EXPECT_TRUE(live.finish().ok());
}

TEST_F(BrewLogRpcTest, UnknownBrewIdClosesStreamWithNotFoundResult)
{
  startServer();
  manager->startBrew(1);
  auto& stream = subscribe(42);
  BrewLog record;
  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_IdNotFound, record.brewlogstop().result());
  EXPECT_FALSE(stream.next(record));
  EXPECT_TRUE(stream.finish().ok());
  manager->stopBrew();
}

TEST_F(BrewLogRpcTest, ChangingProfileAfterBrewKeepsCompletedLogById)
{
  startServer();
  manager->startBrew(1);
  auto& live = subscribe(1);
  BrewLog record;
  ASSERT_TRUE(live.next(record));
  ASSERT_TRUE(live.next(record));
  manager->stopBrew();
  ASSERT_TRUE(live.next(record));
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_Ok, record.brewlogstop().result());
  EXPECT_FALSE(live.next(record));
  EXPECT_TRUE(live.finish().ok());

  struct TempProfileFile {
    std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("openpressod-brew-log-test-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TempProfileFile()
    {
      std::filesystem::remove(path);
    }
  } tempFile;
  auto config =
    OpenpressodConfig::fromString("[global]\nbrew_profile_path = \"" + tempFile.path.string() + "\"");

  auto steamInput = std::make_shared<NiceMock<MockLogicalInput>>();
  auto profiler = std::make_shared<NiceMock<MockBrewProfiler>>();
  auto brewController = std::make_shared<NiceMock<MockTemperatureController>>();
  auto steamController = std::make_shared<NiceMock<MockTemperatureController>>();
  auto powerLed = std::make_shared<NiceMock<MockLogicalOutput>>();
  auto brewLed = std::make_shared<NiceMock<MockLogicalOutput>>();
  auto steamLed = std::make_shared<NiceMock<MockLogicalOutput>>();
  ON_CALL(*core, getLogicalInput(libopenpresso_config_labels::STEAM_BUTTON_LABEL)).WillByDefault(Return(steamInput));
  ON_CALL(*core, getBrewProfiler(libopenpresso_config_labels::BREW_PROFILER_LABEL)).WillByDefault(Return(profiler));
  ON_CALL(*core, getTemperatureController(libopenpresso_config_labels::BREW_TEMPERATURE_CONTROLLER_LABEL))
    .WillByDefault(Return(brewController));
  ON_CALL(*core, getTemperatureController(libopenpresso_config_labels::STEAM_CONTROLLER_LABEL))
    .WillByDefault(Return(steamController));
  ON_CALL(*core, getLogicalOutput(libopenpresso_config_labels::POWER_LED_LABEL)).WillByDefault(Return(powerLed));
  ON_CALL(*core, getLogicalOutput(libopenpresso_config_labels::BREW_LED_LABEL)).WillByDefault(Return(brewLed));
  ON_CALL(*core, getLogicalOutput(libopenpresso_config_labels::STEAM_LED_LABEL)).WillByDefault(Return(steamLed));

  auto leds = std::make_unique<LedsHandler>(core);
  auto status = std::make_unique<StatusManager>(core, config, std::move(leds));
  auto state = std::make_unique<StateManager>(core, std::move(status));
  auto profiles = std::make_unique<BrewProfileManager>(config);
  auto* profilesPtr = profiles.get();
  auto settings = std::make_unique<UserSettingsManager>(config);
  AsyncEventDispatcher dispatcher(std::move(state), std::move(profiles), std::move(settings), manager);

  BrewProfile profile;
  profile.set_name("new profile");
  profile.set_temperature(94'000);
  auto* step = profile.add_steps();
  step->set_targettype(BrewStepTargetType_Pressure);
  step->set_targetvalue(9'000);
  std::promise<void> completed;
  auto future = completed.get_future();
  dispatcher.setBrewProfile(&profile, [&](std::future<void> result) {
    try {
      result.get();
      completed.set_value();
    }
    catch (...) {
      completed.set_exception(std::current_exception());
    }
  });
  ASSERT_EQ(std::future_status::ready, future.wait_for(2s));
  EXPECT_NO_THROW(future.get());
  EXPECT_EQ("new profile", profilesPtr->getProfile().name());

  ON_CALL(*profiler, isActive()).WillByDefault(Return(true));
  BrewProfile rejectedProfile = profile;
  rejectedProfile.set_name("rejected profile");
  std::promise<void> rejected;
  auto rejectedFuture = rejected.get_future();
  dispatcher.setBrewProfile(&rejectedProfile, [&](std::future<void> result) {
    try {
      result.get();
      rejected.set_value();
    }
    catch (...) {
      rejected.set_exception(std::current_exception());
    }
  });
  ASSERT_EQ(std::future_status::ready, rejectedFuture.wait_for(2s));
  EXPECT_THROW(rejectedFuture.get(), std::runtime_error);
  EXPECT_EQ("new profile", profilesPtr->getProfile().name());
  ON_CALL(*profiler, isActive()).WillByDefault(Return(false));

  std::promise<BrewProfileInfo> profileInfoResult;
  auto profileInfoFuture = profileInfoResult.get_future();
  dispatcher.getBrewProfile([&](std::future<BrewProfileInfo> result) {
    try {
      profileInfoResult.set_value(result.get());
    }
    catch (...) {
      profileInfoResult.set_exception(std::current_exception());
    }
  });
  ASSERT_EQ(std::future_status::ready, profileInfoFuture.wait_for(2s));
  const auto profileInfo = profileInfoFuture.get();
  EXPECT_EQ(1U, profileInfo.generation());
  EXPECT_EQ("new profile", profileInfo.profile().name());
  EXPECT_EQ(94'000, profileInfo.profile().temperature());
  ASSERT_EQ(1, profileInfo.profile().steps_size());
  EXPECT_EQ(9'000, profileInfo.profile().steps(0).targetvalue());

  std::promise<BrewState> brewStateResult;
  auto brewStateFuture = brewStateResult.get_future();
  dispatcher.getBrewState([&](std::future<BrewState> result) {
    try {
      brewStateResult.set_value(result.get());
    }
    catch (...) {
      brewStateResult.set_exception(std::current_exception());
    }
  });
  ASSERT_EQ(std::future_status::ready, brewStateFuture.wait_for(2s));
  const auto brewState = brewStateFuture.get();
  EXPECT_FALSE(brewState.isactive());
  EXPECT_EQ(0U, brewState.brewid());
  EXPECT_EQ(profileInfo.generation(), brewState.profilegeneration());

  auto& after = subscribe(1);
  ASSERT_TRUE(after.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(after.next(record));
  EXPECT_TRUE(record.has_record());
  ASSERT_TRUE(after.next(record));
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_Ok, record.brewlogstop().result());
  EXPECT_FALSE(after.next(record));
  EXPECT_TRUE(after.finish().ok());
}

TEST_F(BrewLogRpcTest, StepWithoutMetricsSnapshotIsNotRecorded)
{
  startServer();
  manager->startBrew(1);
  auto& stream = subscribe(1);
  BrewLog record;
  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(record.has_record());
  EXPECT_EQ(0U, record.record().stepindex());
  manager->setCurrentStep(1);
  manager->stopBrew();
  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_Ok, record.brewlogstop().result());
  EXPECT_FALSE(stream.next(record));
  EXPECT_TRUE(stream.finish().ok());
}

TEST_F(BrewLogRpcTest, RetainedLogOmitsStepWithoutMetricsSnapshot)
{
  startServer();
  manager->startBrew(1);
  auto& live = subscribe(1);
  BrewLog record;
  ASSERT_TRUE(live.next(record));
  ASSERT_TRUE(live.next(record));
  ASSERT_TRUE(record.has_record());
  EXPECT_EQ(0U, record.record().stepindex());
  manager->setCurrentStep(1);
  manager->stopBrew();
  ASSERT_TRUE(live.next(record));
  EXPECT_TRUE(record.has_brewlogstop());
  EXPECT_FALSE(live.next(record));
  EXPECT_TRUE(live.finish().ok());

  auto& replay = subscribe(1);
  ASSERT_TRUE(replay.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(replay.next(record));
  EXPECT_TRUE(record.has_record());
  EXPECT_EQ(0U, record.record().stepindex());
  ASSERT_TRUE(replay.next(record));
  ASSERT_TRUE(record.has_brewlogstop());
  EXPECT_EQ(BrewLogResult_Ok, record.brewlogstop().result());
  EXPECT_FALSE(replay.next(record));
  EXPECT_TRUE(replay.finish().ok());
}

} // namespace

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
  using Sample = std::tuple<int, int, int, int>;
  std::vector<Sample> samples;
  auto capture = [&](auto t, auto p, auto w, auto f) {
    {
      std::scoped_lock lock(mutex);
      samples.emplace_back(t, p, w, f);
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
    EXPECT_THAT(samples, Each(Sample{93'000, 8'500, 17'000, 2'100}));
  }
  std::this_thread::sleep_for(30ms);
  {
    std::scoped_lock lock(mutex);
    EXPECT_EQ(stoppedCount, samples.size());
  }

  sampler.startSampling(capture);
  EXPECT_TRUE(waitFor(stoppedCount + 1));
  sampler.stopSampling();
  sampler.stopSampling();
}

class BrewLogRpcTest : public Test {
protected:
  struct Stream {
    grpc::ClientContext context;
    std::unique_ptr<grpc::ClientReader<BrewLogRecord>> reader;

    Stream(OpenpressoDaemon::Stub& stub, std::chrono::milliseconds deadline = 2s)
    {
      context.set_deadline(std::chrono::system_clock::now() + deadline);
      reader = stub.brewLog(&context, google::protobuf::Empty{});
    }

    ~Stream()
    {
      context.TryCancel();
      reader->Finish();
    }

    bool next(BrewLogRecord& record)
    {
      return reader->Read(&record);
    }
  };

  void startServer(const std::string& interval = "00:00:01")
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

    auto config = OpenpressodConfig::fromString("[global]\nbrew_log_sampling_interval = " + interval);
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

  Stream& subscribe(std::chrono::milliseconds deadline = 2s)
  {
    streams.push_back(std::make_unique<Stream>(*stub, deadline));
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

TEST_F(BrewLogRpcTest, StartMetricsStepChangesAndStopAreStreamedInOrder)
{
  startServer("00:00:00.050");
  auto& stream = subscribe();
  const auto before = std::chrono::system_clock::now();
  manager->startBrew();
  BrewLogRecord record;
  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(record.has_brewlogstart());
  EXPECT_EQ(0, record.brewlogstart().samplinginterval().seconds());
  EXPECT_EQ(50'000'000, record.brewlogstart().samplinginterval().nanos());
  const auto started = std::chrono::seconds(record.brewlogstart().startpoint().seconds()) +
                       std::chrono::nanoseconds(record.brewlogstart().startpoint().nanos());
  EXPECT_GE(started, before.time_since_epoch());
  EXPECT_LE(started, std::chrono::system_clock::now().time_since_epoch());

  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(record.has_metrics());
  EXPECT_EQ(93'000, record.metrics().temperature());
  EXPECT_EQ(8'500U, record.metrics().pressure());
  EXPECT_EQ(17'000, record.metrics().weight());
  EXPECT_EQ(2'100, record.metrics().flowrate());

  manager->setCurrentStep(2);
  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(record.has_stepchange());
  EXPECT_EQ(1U, record.stepchange().brewstepindex());
  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(record.has_stepchange());
  EXPECT_EQ(2U, record.stepchange().brewstepindex());
  ASSERT_TRUE(stream.next(record));
  EXPECT_TRUE(record.has_metrics());
  manager->stopBrew();
  ASSERT_TRUE(stream.next(record));
  EXPECT_TRUE(record.has_stopflag());
}

TEST_F(BrewLogRpcTest, LateSubscribersGetCurrentAndCompletedLog)
{
  startServer();
  auto& first = subscribe();
  manager->startBrew();
  BrewLogRecord record;
  ASSERT_TRUE(first.next(record));
  ASSERT_TRUE(first.next(record));
  ASSERT_TRUE(record.has_metrics());

  auto& during = subscribe();
  ASSERT_TRUE(during.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(during.next(record));
  EXPECT_TRUE(record.has_metrics());

  manager->stopBrew();
  ASSERT_TRUE(during.next(record));
  EXPECT_TRUE(record.has_stopflag());
  auto& after = subscribe();
  ASSERT_TRUE(after.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(after.next(record));
  EXPECT_TRUE(record.has_metrics());
  ASSERT_TRUE(after.next(record));
  EXPECT_TRUE(record.has_stopflag());
}

TEST_F(BrewLogRpcTest, NewBrewReplacesPreviouslyRetainedLog)
{
  startServer();
  manager->startBrew();
  manager->stopBrew();
  ON_CALL(*temperature, getTemperature()).WillByDefault(Return(94'000));
  manager->startBrew();
  manager->stopBrew();

  auto& replay = subscribe();
  BrewLogRecord record;
  ASSERT_TRUE(replay.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(replay.next(record));
  ASSERT_TRUE(record.has_metrics());
  EXPECT_EQ(94'000, record.metrics().temperature());
  ASSERT_TRUE(replay.next(record));
  EXPECT_TRUE(record.has_stopflag());
}

TEST_F(BrewLogRpcTest, CancellingOneSubscriberDoesNotStopAnother)
{
  startServer();
  auto& cancelled = subscribe();
  auto& live = subscribe();
  cancelled.context.TryCancel();
  manager->startBrew();
  BrewLogRecord record;
  ASSERT_TRUE(live.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(live.next(record));
  EXPECT_TRUE(record.has_metrics());
  manager->stopBrew();
  ASSERT_TRUE(live.next(record));
  EXPECT_TRUE(record.has_stopflag());
}

TEST_F(BrewLogRpcTest, ResetNotifiesSubscribersAndClearsReplay)
{
  startServer();
  auto& live = subscribe();
  manager->startBrew();
  BrewLogRecord record;
  ASSERT_TRUE(live.next(record));
  ASSERT_TRUE(live.next(record));
  manager->stopBrew();
  ASSERT_TRUE(live.next(record));
  EXPECT_TRUE(record.has_stopflag());
  manager->resetLog();
  ASSERT_TRUE(live.next(record));
  EXPECT_TRUE(record.has_logcleanedup());

  auto& after = subscribe(250ms);
  EXPECT_FALSE(after.next(record));
}

TEST_F(BrewLogRpcTest, ChangingProfileAfterBrewClearsOutdatedLog)
{
  startServer();
  auto& live = subscribe();
  manager->startBrew();
  BrewLogRecord record;
  ASSERT_TRUE(live.next(record));
  ASSERT_TRUE(live.next(record));
  manager->stopBrew();
  ASSERT_TRUE(live.next(record));
  ASSERT_TRUE(record.has_stopflag());

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
  ASSERT_TRUE(live.next(record));
  EXPECT_TRUE(record.has_logcleanedup());
  auto& after = subscribe(250ms);
  EXPECT_FALSE(after.next(record));
}

TEST_F(BrewLogRpcTest, StepEventWithoutFollowingSampleStillNeedsMarker)
{
  startServer();
  auto& stream = subscribe();
  manager->startBrew();
  BrewLogRecord record;
  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(stream.next(record));
  manager->setCurrentStep(1);
  manager->stopBrew();
  ASSERT_TRUE(stream.next(record));
  ASSERT_TRUE(record.has_stepchange()) << "step event was lost before the next metrics sample";
  EXPECT_EQ(1U, record.stepchange().brewstepindex());
  ASSERT_TRUE(stream.next(record));
  EXPECT_TRUE(record.has_stopflag());
}

TEST_F(BrewLogRpcTest, LateSubscriberGetsFinalStepWithoutFollowingSample)
{
  startServer();
  auto& live = subscribe();
  manager->startBrew();
  BrewLogRecord record;
  ASSERT_TRUE(live.next(record));
  ASSERT_TRUE(live.next(record));
  ASSERT_TRUE(record.has_metrics());
  manager->setCurrentStep(1);
  manager->stopBrew();

  auto& replay = subscribe();
  ASSERT_TRUE(replay.next(record));
  EXPECT_TRUE(record.has_brewlogstart());
  ASSERT_TRUE(replay.next(record));
  EXPECT_TRUE(record.has_metrics());
  ASSERT_TRUE(replay.next(record));
  ASSERT_TRUE(record.has_stepchange()) << "retained log lost the final step event";
  EXPECT_EQ(1U, record.stepchange().brewstepindex());
  ASSERT_TRUE(replay.next(record));
  EXPECT_TRUE(record.has_stopflag());
}

} // namespace

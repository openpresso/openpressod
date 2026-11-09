#include "brew_history/brew_history_sampler.hpp"
#include "config/libopenpresso_config_labels.hpp"
#include "libopenpresso_core_mock.hpp"
#include "pressure_sensor_mock.hpp"
#include "weight_sensor_mock.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <future>
#include <memory>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace openpressod;
using namespace std::chrono_literals;
using testing::Return;
using testing::StrictMock;

namespace
{

class BrewHistorySamplerTest : public testing::Test {
protected:
  using Sample =
    std::tuple<libopenpresso::millibars_t, libopenpresso::milligrams_t, libopenpresso::milligrams_p_second_t, size_t>;

  void SetUp() override
  {
    EXPECT_CALL(*core, getPressureSensor(libopenpresso_config_labels::PRESSURE_SENSOR_LABEL))
      .WillOnce(Return(pressure));
    EXPECT_CALL(*core, getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL)).WillOnce(Return(weight));
    EXPECT_CALL(*pressure, getPressure()).WillRepeatedly(Return(8'500));
    EXPECT_CALL(*weight, getWeight()).WillRepeatedly(Return(-200));
    EXPECT_CALL(*weight, getFlowRate()).WillRepeatedly(Return(-300));
  }

  void TearDown() override
  {
    // Join before destroying any state captured by the sampling callback, including on assertion
    // failure.
    if (sampler) {
      sampler->stopSampling();
    }
  }

  void start(std::chrono::steady_clock::duration interval = 10ms)
  {
    sampler = std::make_unique<BrewHistorySampler>(core, interval);
    sampler->startSampling([this](auto... values) { capture(values...); });
  }

  void capture(auto... values)
  {
    {
      std::scoped_lock lock(mutex);
      samples.emplace_back(values...);
      times.push_back(std::chrono::steady_clock::now());
    }
    sampled.notify_one();
  }

  bool waitFor(size_t count)
  {
    std::unique_lock lock(mutex);
    return sampled.wait_for(lock, 2s, [&] { return samples.size() >= count; });
  }

  std::shared_ptr<StrictMock<MockLibopenpressoCore>> core =
    std::make_shared<StrictMock<MockLibopenpressoCore>>();
  std::shared_ptr<StrictMock<MockPressureSensor>> pressure =
    std::make_shared<StrictMock<MockPressureSensor>>();
  std::shared_ptr<StrictMock<MockWeightSensor>> weight =
    std::make_shared<StrictMock<MockWeightSensor>>();
  std::mutex mutex;
  std::condition_variable sampled;
  std::vector<Sample> samples;
  std::vector<std::chrono::steady_clock::time_point> times;
  std::unique_ptr<BrewHistorySampler> sampler;
};

TEST_F(BrewHistorySamplerTest, FirstSampleIsImmediateAndPreservesSignedMetrics)
{
  // A long interval distinguishes an immediate sample from waiting for the first tick.
  start(1h);
  ASSERT_TRUE(waitFor(1));
  sampler->stopSampling();
  EXPECT_THAT(samples, testing::ElementsAre(Sample{8'500, -200, -300, 0}));
}

TEST_F(BrewHistorySamplerTest, SamplesPeriodicallyAtTheConfiguredInterval)
{
  const auto before = std::chrono::steady_clock::now();
  start(30ms);
  ASSERT_TRUE(waitFor(4));
  sampler->stopSampling();
  ASSERT_GE(times.size(), 4U);
  // Measure from before launch so scheduler delays cannot make valid catch-up ticks fail.
  EXPECT_GE(times[3] - before, 80ms);
  EXPECT_THAT(samples, testing::Each(Sample{8'500, -200, -300, 0}));
}

TEST_F(BrewHistorySamplerTest, ReadsFreshMetricsAndCurrentStepOnEachTick)
{
  EXPECT_CALL(*pressure, getPressure()).WillOnce(Return(1'000)).WillRepeatedly(Return(9'000));
  EXPECT_CALL(*weight, getWeight()).WillOnce(Return(-100)).WillRepeatedly(Return(36'000));
  EXPECT_CALL(*weight, getFlowRate()).WillOnce(Return(-200)).WillRepeatedly(Return(2'100));
  start();
  ASSERT_TRUE(waitFor(1));
  sampler->setCurrentStep(7);
  {
    std::unique_lock lock(mutex);
    ASSERT_TRUE(sampled.wait_for(lock, 2s, [&] { return std::get<3>(samples.back()) == 7; }));
  }
  sampler->stopSampling();
  EXPECT_EQ((Sample{1'000, -100, -200, 0}), samples.front());
  EXPECT_EQ((Sample{9'000, 36'000, 2'100, 7}), samples.back());
}

TEST_F(BrewHistorySamplerTest, StopInterruptsTheWaitAndPreventsFurtherSensorReads)
{
  start(1h);
  ASSERT_TRUE(waitFor(1));
  const auto before = std::chrono::steady_clock::now();
  sampler->stopSampling();
  EXPECT_LT(std::chrono::steady_clock::now() - before, 1s);
  testing::Mock::VerifyAndClearExpectations(pressure.get());
  testing::Mock::VerifyAndClearExpectations(weight.get());
  EXPECT_CALL(*pressure, getPressure()).Times(0);
  EXPECT_CALL(*weight, getWeight()).Times(0);
  EXPECT_CALL(*weight, getFlowRate()).Times(0);
  std::unique_lock lock(mutex);
  EXPECT_FALSE(sampled.wait_for(lock, 50ms, [&] { return samples.size() > 1; }));
}

TEST_F(BrewHistorySamplerTest, StopBeforeStartAndRepeatedStopAreHarmless)
{
  sampler = std::make_unique<BrewHistorySampler>(core, 1h);
  EXPECT_NO_THROW(sampler->stopSampling());
  EXPECT_NO_THROW(sampler->stopSampling());
  sampler->startSampling([this](auto... values) { capture(values...); });
  ASSERT_TRUE(waitFor(1));
  EXPECT_NO_THROW(sampler->stopSampling());
  EXPECT_NO_THROW(sampler->stopSampling());
}

TEST_F(BrewHistorySamplerTest, RestartResetsStepAndUsesTheNewMoveOnlyCallback)
{
  start(1h);
  ASSERT_TRUE(waitFor(1));
  sampler->stopSampling();
  sampler->setCurrentStep(99);
  sampler->startSampling([this, marker = std::make_unique<int>(42)](auto... values) {
    EXPECT_EQ(42, *marker);
    capture(values...);
  });
  ASSERT_TRUE(waitFor(2));
  sampler->stopSampling();
  EXPECT_THAT(samples, testing::ElementsAre(Sample{8'500, -200, -300, 0}, Sample{8'500, -200, -300, 0}));
}

TEST_F(BrewHistorySamplerTest, StopWaitsForAnInFlightCallbackToFinish)
{
  std::promise<void> entered;
  auto enteredFuture = entered.get_future();
  std::promise<void> release;
  auto released = release.get_future();
  sampler = std::make_unique<BrewHistorySampler>(core, 1h);
  sampler->startSampling([&](auto...) {
    entered.set_value();
    released.wait();
  });
  // Release even if entry fails, so teardown can always join the sampling thread.
  const auto status = enteredFuture.wait_for(2s);
  if (status != std::future_status::ready) {
    release.set_value();
    sampler->stopSampling();
    FAIL() << "Sampling callback did not start";
  }
  std::promise<void> stopping;
  auto stoppingFuture = stopping.get_future();
  auto stopped = std::async(std::launch::async, [&] {
    stopping.set_value();
    sampler->stopSampling();
  });
  stoppingFuture.wait();
  EXPECT_EQ(std::future_status::timeout, stopped.wait_for(50ms));
  release.set_value();
  ASSERT_EQ(std::future_status::ready, stopped.wait_for(2s));
  EXPECT_NO_THROW(stopped.get());
}

} // namespace

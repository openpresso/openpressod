#include "brew_profiler_mock.hpp"
#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"
#include "leds_handler_mock.hpp"
#include "libopenpresso_core_mock.hpp"
#include "logical_input_mock.hpp"
#include "state_manager/brew_timer.hpp"
#include "state_manager/state_manager.hpp"
#include "temperature_controller_mock.hpp"
#include "weight_sensor_mock.hpp"

#include <thread>

#include <gtest/gtest.h>
#include <openpresso_proto/openpresso.pb.h>

using namespace testing;
using namespace libopenpresso::interfaces;
using namespace openpressod;

constexpr auto test_config_toml = R"(
[brew]
temperature = 93000
pressure = 9000
)";

class StateManagerTest : public Test {
protected:
  std::shared_ptr<NiceMock<MockLibopenpressoCore>> core;
  std::shared_ptr<NiceMock<MockLogicalInput>> steamButton;
  std::shared_ptr<NiceMock<MockLogicalInput>> powerButton;
  std::shared_ptr<NiceMock<MockBrewProfiler>> brewProfiler;
  std::shared_ptr<NiceMock<MockTemperatureController>> steamController;
  std::shared_ptr<NiceMock<MockTemperatureController>> temperatureController;
  std::shared_ptr<NiceMock<MockWeightSensor>> weightSensor;

  openpressod::LedsHandlerMock* ledsHandler = nullptr;

  openpressod::OpenpressodConfig config;

  StateManagerTest()
  : core(std::make_shared<NiceMock<MockLibopenpressoCore>>())
  , steamButton(std::make_shared<NiceMock<MockLogicalInput>>())
  , powerButton(std::make_shared<NiceMock<MockLogicalInput>>())
  , brewProfiler(std::make_shared<NiceMock<MockBrewProfiler>>())
  , steamController(std::make_shared<NiceMock<MockTemperatureController>>())
  , temperatureController(std::make_shared<NiceMock<MockTemperatureController>>())
  , weightSensor(std::make_shared<NiceMock<MockWeightSensor>>())
  , config(openpressod::OpenpressodConfig::fromString(test_config_toml))
  {
  }

  void SetUp() override
  {
    ON_CALL(*core, getLogicalInput(libopenpresso_config_labels::STEAM_BUTTON_LABEL)).WillByDefault(Return(steamButton));
    ON_CALL(*core, getLogicalInput(libopenpresso_config_labels::POWER_BUTTON_LABEL)).WillByDefault(Return(powerButton));
    ON_CALL(*core, getBrewProfiler(libopenpresso_config_labels::BREW_PROFILER_LABEL))
      .WillByDefault(Return(brewProfiler));
    ON_CALL(*core, getTemperatureController(libopenpresso_config_labels::STEAM_CONTROLLER_LABEL))
      .WillByDefault(Return(steamController));
    ON_CALL(*core, getTemperatureController(libopenpresso_config_labels::BREW_TEMPERATURE_CONTROLLER_LABEL))
      .WillByDefault(Return(temperatureController));
    ON_CALL(*core, getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL))
      .WillByDefault(Return(weightSensor));

    ON_CALL(*temperatureController, getTargetTemperature()).WillByDefault(Return(config.brewTemperature()));
    ON_CALL(*steamController, getTargetTemperature()).WillByDefault(Return(config.steamTemperature()));

    ON_CALL(*steamButton, getState()).WillByDefault(Return(false));
    ON_CALL(*powerButton, getState()).WillByDefault(Return(false));
    ON_CALL(*brewProfiler, isActive()).WillByDefault(Return(false));
  }

  std::unique_ptr<openpressod::StateManager> createManager()
  {
    auto leds = std::make_unique<NiceMock<openpressod::LedsHandlerMock>>();
    ledsHandler = leds.get();
    return std::make_unique<openpressod::StateManager>(core, std::move(leds));
  }
};

TEST_F(StateManagerTest, Construct)
{
  EXPECT_CALL(*core, getLogicalInput(libopenpresso_config_labels::STEAM_BUTTON_LABEL));
  EXPECT_CALL(*core, getBrewProfiler(libopenpresso_config_labels::BREW_PROFILER_LABEL));
  EXPECT_CALL(*core, getTemperatureController(libopenpresso_config_labels::STEAM_CONTROLLER_LABEL));
  EXPECT_CALL(*core, getTemperatureController(libopenpresso_config_labels::BREW_TEMPERATURE_CONTROLLER_LABEL));
  EXPECT_CALL(*core, getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL));

  auto manager = createManager();

  EXPECT_FALSE(manager->getPowerState());
  EXPECT_FALSE(manager->getBrewState());
  EXPECT_FALSE(manager->getSteamModeState());
}

TEST_F(StateManagerTest, ConstructWithPowerButtonHigh)
{
  EXPECT_CALL(*powerButton, getState()).WillRepeatedly(Return(true));

  auto manager = createManager();

  // Power state must remain false despite button state initially
  EXPECT_FALSE(manager->getPowerState());
}

TEST_F(StateManagerTest, SetPowerState)
{
  auto manager = createManager();

  // Test turn power ON in brew mode
  EXPECT_CALL(*temperatureController, activate());
  EXPECT_CALL(*ledsHandler, indicateBrewState(config.brewTemperature()));
  manager->setPowerState(true);
  EXPECT_TRUE(manager->getPowerState());

  // Test turn power OFF
  EXPECT_CALL(*brewProfiler, deactivate());
  EXPECT_CALL(*steamController, deactivate());
  EXPECT_CALL(*temperatureController, deactivate());
  EXPECT_CALL(*ledsHandler, indicatePowerOff());
  manager->setPowerState(false);
  EXPECT_FALSE(manager->getPowerState());

  // Test turn power ON in steam mode
  EXPECT_CALL(*steamButton, getState()).WillRepeatedly(Return(true));
  // Re-create manager so it initializes with steam mode = true
  manager = createManager();

  EXPECT_TRUE(manager->getSteamModeState());

  EXPECT_CALL(*steamController, activate());
  EXPECT_CALL(*ledsHandler, indicateSteamState(_));
  manager->setPowerState(true);
  EXPECT_TRUE(manager->getPowerState());
}

TEST_F(StateManagerTest, StartStopBrew)
{
  auto manager = createManager();

  // Throws if power is off
  EXPECT_THROW(manager->startBrew(), std::runtime_error);

  EXPECT_CALL(*temperatureController, activate());
  manager->setPowerState(true);

  // Success
  EXPECT_CALL(*brewProfiler, activate());
  manager->startBrew();

  // Stop brew
  EXPECT_CALL(*brewProfiler, isActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(*brewProfiler, deactivate());
  manager->stopBrew();
}

TEST_F(StateManagerTest, SetSteamModeState)
{
  auto manager = createManager();

  // When power is OFF, changing steam mode shouldn't activate controllers
  EXPECT_CALL(*steamController, activate()).Times(0);
  EXPECT_CALL(*ledsHandler, indicateSteamState(_)).Times(0);
  manager->setSteamModeState(true);
  EXPECT_TRUE(manager->getSteamModeState());

  // Switching back to brew mode while power is OFF also shouldn't activate controllers
  EXPECT_CALL(*temperatureController, activate()).Times(0);
  EXPECT_CALL(*ledsHandler, indicateBrewState(_)).Times(0);
  manager->setSteamModeState(false);
  EXPECT_FALSE(manager->getSteamModeState());

  // Restore steam mode to true so we can test turning power ON in steam mode
  manager->setSteamModeState(true);

  // Now turn power ON, it should activate steam controller
  EXPECT_CALL(*steamController, activate());
  EXPECT_CALL(*ledsHandler, indicateSteamState(_));
  manager->setPowerState(true);

  // Switch to brew mode
  EXPECT_CALL(*steamController, deactivate());
  EXPECT_CALL(*temperatureController, activate());
  EXPECT_CALL(*ledsHandler, indicateBrewState(config.brewTemperature()));
  manager->setSteamModeState(false);
  EXPECT_FALSE(manager->getSteamModeState());

  // If brewing, cannot switch
  EXPECT_CALL(*brewProfiler, isActive()).WillRepeatedly(Return(true));
  EXPECT_THROW(manager->setSteamModeState(true), std::runtime_error);
}

TEST_F(StateManagerTest, ResetScales)
{
  auto manager = createManager();

  EXPECT_CALL(*weightSensor, tare());
  manager->resetScales();

  EXPECT_CALL(*brewProfiler, isActive()).WillRepeatedly(Return(true));
  EXPECT_THROW(manager->resetScales(), std::runtime_error);
}

TEST_F(StateManagerTest, SetBrewTemperaturePowerOff)
{
  auto manager = createManager();

  EXPECT_CALL(*temperatureController, setTargetTemperature(95000));
  EXPECT_CALL(*ledsHandler, indicateBrewState(_)).Times(0);
  manager->setBrewTemperature(95000);
}

TEST_F(StateManagerTest, SetBrewTemperaturePowerOnNoSteam)
{
  auto manager = createManager();
  manager->setPowerState(true);

  EXPECT_CALL(*temperatureController, setTargetTemperature(96000));
  EXPECT_CALL(*ledsHandler, indicateBrewState(96000));
  manager->setBrewTemperature(96000);
}

TEST_F(StateManagerTest, SetBrewTemperaturePowerOnSteam)
{
  auto manager = createManager();
  manager->setSteamModeState(true);
  manager->setPowerState(true);

  EXPECT_CALL(*temperatureController, setTargetTemperature(97000));
  EXPECT_CALL(*ledsHandler, indicateBrewState(_)).Times(0);
  manager->setBrewTemperature(97000);
}

TEST_F(StateManagerTest, SetSteamTemperaturePowerOff)
{
  auto manager = createManager();

  EXPECT_CALL(*steamController, setTargetTemperature(160000));
  EXPECT_CALL(*ledsHandler, indicateSteamState(_)).Times(0);
  manager->setSteamTemperature(160000);
}

TEST_F(StateManagerTest, SetSteamTemperaturePowerOnNoSteam)
{
  auto manager = createManager();
  manager->setPowerState(true);

  EXPECT_CALL(*steamController, setTargetTemperature(160000));
  EXPECT_CALL(*ledsHandler, indicateSteamState(_)).Times(0);
  manager->setSteamTemperature(160000);
}

TEST_F(StateManagerTest, SetSteamTemperaturePowerOnSteam)
{
  auto manager = createManager();
  manager->setSteamModeState(true);
  manager->setPowerState(true);

  EXPECT_CALL(*steamController, setTargetTemperature(160000));
  EXPECT_CALL(*ledsHandler, indicateSteamState(160000));
  manager->setSteamTemperature(160000);
}

TEST_F(StateManagerTest, SetBrewSteps)
{
  using namespace libopenpresso::brew_step_targets;
  using namespace libopenpresso::brew_step_advance_conditions;

  auto manager = createManager();
  std::vector<std::pair<libopenpresso::step_target_t, libopenpresso::next_step_condition_t>> steps;
  steps.push_back({ConstantPressure{8000}, OnStepTime{std::chrono::seconds{10}}});

  EXPECT_CALL(*brewProfiler, setSteps(_)).WillOnce([](const auto& set_steps) {
    EXPECT_EQ(set_steps.size(), 1);
    EXPECT_TRUE(std::holds_alternative<ConstantPressure>(set_steps[0].first));
    EXPECT_EQ(std::get<ConstantPressure>(set_steps[0].first).pressure, 8000);
    EXPECT_TRUE(std::holds_alternative<OnStepTime>(set_steps[0].second));
  });

  manager->setBrewSteps(steps);

  EXPECT_CALL(*brewProfiler, isActive()).WillRepeatedly(Return(true));
  EXPECT_THROW(manager->setBrewSteps(steps), std::runtime_error);
}

TEST_F(StateManagerTest, SetAutoStopCondition)
{
  using namespace libopenpresso::brew_step_advance_conditions;

  auto manager = createManager();
  OnWeight cond{36000};

  EXPECT_CALL(*brewProfiler, setAutoStopCondition(Matcher<OnWeight>(_))).WillOnce([](const OnWeight& set_cond) {
    EXPECT_EQ(set_cond.weight, 36000);
  });

  manager->setAutoStopCondition(cond);

  EXPECT_CALL(*brewProfiler, isActive()).WillRepeatedly(Return(true));
  EXPECT_THROW(manager->setAutoStopCondition(cond), std::runtime_error);
}

TEST_F(StateManagerTest, BrewTimer)
{
  BrewTimer timer;

  EXPECT_EQ(timer.elapsedTime(), 0s);

  timer.start();
  std::this_thread::sleep_for(1500ms);
  auto time = timer.elapsedTime();
  timer.stop();

  EXPECT_GE(time, 1490ms);
  EXPECT_LE(time, 1510ms);

  time = timer.elapsedTime();

  EXPECT_GE(time, 1490ms);
  EXPECT_LE(time, 1510ms);
}
#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"
#include "leds/leds_handler.hpp"
#include "libopenpresso_core_mock.hpp"
#include "logical_output_mock.hpp"
#include "state_manager/status_manager.hpp"
#include "temperature_sensor_mock.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <libopenpresso/interfaces/temperature_sensor.hpp>
#include <openpresso_proto/openpresso.pb.h>

using namespace std::chrono_literals;
using namespace openpressod;
using namespace testing;

namespace
{

constexpr libopenpresso::millidegrees_t TARGET_TEMPERATURE = 95'000;
constexpr libopenpresso::millidegrees_t IN_RANGE_TEMPERATURE = 95'000;
constexpr libopenpresso::millidegrees_t PREHEAT_TEMPERATURE = 94'000;
constexpr libopenpresso::millidegrees_t OVERHEAT_TEMPERATURE = 96'000;

class StatusManagerTest : public Test {
protected:
  std::unique_ptr<StatusManager> makeManager(const std::string& extraConfig = {})
  {
    EXPECT_CALL(*m_core, getLogicalOutput(libopenpresso_config_labels::POWER_LED_LABEL)).WillOnce(Return(m_powerLed));
    EXPECT_CALL(*m_core, getLogicalOutput(libopenpresso_config_labels::BREW_LED_LABEL)).WillOnce(Return(m_brewLed));
    EXPECT_CALL(*m_core, getLogicalOutput(libopenpresso_config_labels::STEAM_LED_LABEL)).WillOnce(Return(m_steamLed));
    EXPECT_CALL(*m_core, getTemperatureSensor(libopenpresso_config_labels::TEMPERATURE_SENSOR_LABEL))
      .WillOnce(Return(m_temperatureSensor));

    auto leds = std::make_unique<LedsHandler>(m_core);
    auto config = OpenpressodConfig::fromString(extraConfig);
    return std::make_unique<StatusManager>(m_core, config, std::move(leds));
  }

  std::shared_ptr<NiceMock<MockLibopenpressoCore>> m_core =
    std::make_shared<NiceMock<MockLibopenpressoCore>>();
  std::shared_ptr<NiceMock<MockTemperatureSensor>> m_temperatureSensor =
    std::make_shared<NiceMock<MockTemperatureSensor>>();
  std::shared_ptr<NiceMock<MockLogicalOutput>> m_powerLed =
    std::make_shared<NiceMock<MockLogicalOutput>>();
  std::shared_ptr<NiceMock<MockLogicalOutput>> m_brewLed =
    std::make_shared<NiceMock<MockLogicalOutput>>();
  std::shared_ptr<NiceMock<MockLogicalOutput>> m_steamLed =
    std::make_shared<NiceMock<MockLogicalOutput>>();
};

TEST_F(StatusManagerTest, AppliesInitialBrewStatusBeforeRegisteringCallback)
{
  auto manager = makeManager();
  libopenpresso::interfaces::TemperatureSensor::callback_t callback;
  std::vector<HeatingStatus> notifications;
  manager->setCallback([&notifications](auto status) { notifications.push_back(status); });

  {
    InSequence sequence;
    EXPECT_CALL(*m_temperatureSensor, getTemperature()).WillOnce(Return(IN_RANGE_TEMPERATURE));
    EXPECT_CALL(*m_powerLed, setState(true));
    EXPECT_CALL(*m_brewLed, setState(true));
    EXPECT_CALL(*m_steamLed, setState(false));
    EXPECT_CALL(*m_temperatureSensor, fixedUpdateRate()).WillOnce(Return(100ms));
    EXPECT_CALL(*m_temperatureSensor, registerCallback(_)).WillOnce(DoAll(SaveArg<0>(&callback), Return(1)));
  }

  manager->indicateBrewState(TARGET_TEMPERATURE);

  EXPECT_EQ(HeatingStatus::HeatingStatus_Ready, manager->getCurrentStatus());
  EXPECT_THAT(notifications, ElementsAre(HeatingStatus::HeatingStatus_Ready));
  EXPECT_TRUE(static_cast<bool>(callback));
}

TEST_F(StatusManagerTest, SteamUsesSteamWindowAndSteamReadyLed)
{
  auto manager = makeManager();
  libopenpresso::interfaces::TemperatureSensor::callback_t callback;

  EXPECT_CALL(*m_powerLed, setState(true));
  EXPECT_CALL(*m_brewLed, setState(false));
  EXPECT_CALL(*m_steamLed, setState(true));
  EXPECT_CALL(*m_temperatureSensor, getTemperature()).WillOnce(Return(154'100));
  EXPECT_CALL(*m_temperatureSensor, fixedUpdateRate()).WillOnce(Return(100ms));
  EXPECT_CALL(*m_temperatureSensor, registerCallback(_)).WillOnce(DoAll(SaveArg<0>(&callback), Return(1)));

  manager->indicateSteamState(155'000);

  EXPECT_EQ(HeatingStatus::HeatingStatus_Ready, manager->getCurrentStatus());
}

TEST_F(StatusManagerTest, FixedRateCallbackCountsConsecutiveInRangeInvocations)
{
  auto manager = makeManager(R"(
[heater]
ready_hysteresis_period = 00:00:00.250
)");
  libopenpresso::interfaces::TemperatureSensor::callback_t callback;

  EXPECT_CALL(*m_temperatureSensor, getTemperature()).WillOnce(Return(PREHEAT_TEMPERATURE));
  EXPECT_CALL(*m_temperatureSensor, fixedUpdateRate()).Times(1).WillOnce(Return(100ms));
  EXPECT_CALL(*m_temperatureSensor, registerCallback(_)).WillOnce(DoAll(SaveArg<0>(&callback), Return(1)));
  manager->indicateBrewState(TARGET_TEMPERATURE);

  callback(IN_RANGE_TEMPERATURE);
  callback(IN_RANGE_TEMPERATURE);
  EXPECT_EQ(HeatingStatus::HeatingStatus_Preheat, manager->getCurrentStatus());

  callback(PREHEAT_TEMPERATURE);
  callback(IN_RANGE_TEMPERATURE);
  callback(IN_RANGE_TEMPERATURE);
  EXPECT_EQ(HeatingStatus::HeatingStatus_Preheat, manager->getCurrentStatus());

  callback(IN_RANGE_TEMPERATURE);
  EXPECT_EQ(HeatingStatus::HeatingStatus_Ready, manager->getCurrentStatus());

  callback(OVERHEAT_TEMPERATURE);
  EXPECT_EQ(HeatingStatus::HeatingStatus_Overheat, manager->getCurrentStatus());
}

TEST_F(StatusManagerTest, VariableRateCallbackUsesSteadyClockAndResetsElapsedTime)
{
  auto manager = makeManager(R"(
[heater]
ready_hysteresis_period = 00:00:00.020
)");
  libopenpresso::interfaces::TemperatureSensor::callback_t callback;

  EXPECT_CALL(*m_temperatureSensor, getTemperature()).WillOnce(Return(PREHEAT_TEMPERATURE));
  EXPECT_CALL(*m_temperatureSensor, fixedUpdateRate()).Times(1).WillOnce(Return(std::nullopt));
  EXPECT_CALL(*m_temperatureSensor, registerCallback(_)).WillOnce(DoAll(SaveArg<0>(&callback), Return(1)));
  manager->indicateBrewState(TARGET_TEMPERATURE);

  callback(IN_RANGE_TEMPERATURE);
  callback(IN_RANGE_TEMPERATURE);
  EXPECT_EQ(HeatingStatus::HeatingStatus_Preheat, manager->getCurrentStatus());

  std::this_thread::sleep_for(10ms);
  callback(PREHEAT_TEMPERATURE);
  callback(IN_RANGE_TEMPERATURE);
  std::this_thread::sleep_for(10ms);
  callback(IN_RANGE_TEMPERATURE);
  EXPECT_EQ(HeatingStatus::HeatingStatus_Preheat, manager->getCurrentStatus());

  std::this_thread::sleep_for(15ms);
  callback(IN_RANGE_TEMPERATURE);
  EXPECT_EQ(HeatingStatus::HeatingStatus_Ready, manager->getCurrentStatus());
}

TEST_F(StatusManagerTest, ZeroPeriodBecomesReadyOnFirstInRangeCallback)
{
  auto manager = makeManager(R"(
[heater]
ready_hysteresis_period = 00:00:00
)");
  libopenpresso::interfaces::TemperatureSensor::callback_t callback;

  EXPECT_CALL(*m_temperatureSensor, getTemperature()).WillOnce(Return(PREHEAT_TEMPERATURE));
  EXPECT_CALL(*m_temperatureSensor, fixedUpdateRate()).WillOnce(Return(100ms));
  EXPECT_CALL(*m_temperatureSensor, registerCallback(_)).WillOnce(DoAll(SaveArg<0>(&callback), Return(1)));
  manager->indicateBrewState(TARGET_TEMPERATURE);

  callback(IN_RANGE_TEMPERATURE);
  EXPECT_EQ(HeatingStatus::HeatingStatus_Ready, manager->getCurrentStatus());
}

TEST_F(StatusManagerTest, ReplacesCallbackAndDisableUnregistersAndPublishesDisabled)
{
  auto manager = makeManager();
  std::vector<HeatingStatus> notifications;
  manager->setCallback([&notifications](auto status) { notifications.push_back(status); });

  EXPECT_CALL(*m_temperatureSensor, getTemperature()).Times(2).WillRepeatedly(Return(PREHEAT_TEMPERATURE));
  EXPECT_CALL(*m_temperatureSensor, fixedUpdateRate()).Times(2).WillRepeatedly(Return(100ms));
  EXPECT_CALL(*m_temperatureSensor, registerCallback(_)).WillOnce(Return(10)).WillOnce(Return(11));
  manager->indicateBrewState(TARGET_TEMPERATURE);

  EXPECT_CALL(*m_temperatureSensor, unregisterCallback(10));
  manager->indicateSteamState(155'000);

  EXPECT_CALL(*m_temperatureSensor, unregisterCallback(11));
  manager->disable();

  EXPECT_EQ(HeatingStatus::HeatingStatus_DISABLED, manager->getCurrentStatus());
  EXPECT_THAT(notifications,
              ElementsAre(HeatingStatus::HeatingStatus_Preheat, HeatingStatus::HeatingStatus_DISABLED));
}

} // namespace

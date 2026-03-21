#include "config/libopenpresso_config_constants.hpp"
#include "config/libopenpresso_config_labels.hpp"
#include "config/libopenpresso_config_maker.hpp"
#include "full_config.hpp"
#include "minimal_config.hpp"

#include <variant>

#include <gtest/gtest.h>

using namespace openpressod;
using namespace openpressod::libopenpresso_config_labels;
using namespace openpressod::libopenpresso_config_constants;
using namespace std::chrono_literals;

class LibopenpressoConfigMakerTest : public ::testing::Test {
protected:
  // Helper to retrieve and cast a component from the DeviceConfig map
  template <typename T>
  const T* getComponent(const libopenpresso::DeviceConfig& config, const std::string& label)
  {
    auto it = config.components.find(label);
    if (it == config.components.end()) {
      return nullptr;
    }
    return std::get_if<T>(&it->second);
  }
};

TEST_F(LibopenpressoConfigMakerTest, ConstructsAllComponents)
{
  auto daemonConfig = OpenpressodConfig::fromString(MINIMAL_CONFIG);
  LibopenpressoConfigMaker maker(daemonConfig, nullptr);
  auto deviceConfig = maker.make();

  const std::vector<std::string> expectedLabels = {
    PRESSURE_SENSOR_LABEL,
    TEMPERATURE_SENSOR_LABEL,
    AC_SENSOR_LABEL,
    WEIGHT_SENSOR_LABEL,
    INTERNAL_FLOW_SENSOR_LABEL,
    POWER_BUTTON_LABEL,
    BREW_BUTTON_LABEL,
    STEAM_BUTTON_LABEL,
    POWER_LED_LABEL,
    BREW_LED_LABEL,
    STEAM_LED_LABEL,
    PUMP_DRIVER_LABEL,
    HEATER_DRIVER_LABEL,
    VALVE_DRIVER_LABEL,
    PUMP_POWER_CONTROLLER_LABEL,
    HEATER_POWER_CONTROLLER_LABEL,
    PRESSURE_CONTROLLER_LABEL,
    FLOW_RATE_CONTROLLER_LABEL,
    INTERNAL_FLOW_RATE_CONTROLLER_LABEL,
    BREW_TEMPERATURE_CONTROLLER_LABEL,
    STEAM_PREHEAT_TEMPERATURE_CONTROLLER_LABEL,
    STEAM_ACTIVE_TEMPERATURE_CONTROLLER_LABEL,
    BREW_PROFILER_LABEL,
    STEAM_CONTROLLER_LABEL,
  };

  EXPECT_EQ(expectedLabels.size(), deviceConfig.components.size());

  for (const auto& label : expectedLabels) {
    EXPECT_TRUE(deviceConfig.components.contains(label)) << "Missing component: " << label;
  }
}

TEST_F(LibopenpressoConfigMakerTest, PopulatesComponentConfigsCorrectly_FullConfig)
{
  auto daemonConfig = OpenpressodConfig::fromString(FULL_CONFIG);
  LibopenpressoConfigMaker maker(daemonConfig, nullptr);
  auto deviceConfig = maker.make();

  // --- Watchdog ---

  EXPECT_FALSE(deviceConfig.watchdog.has_value());

  // --- Sensors ---

  auto pressureSensor =
    getComponent<libopenpresso::Ads1115PressureSensorConfig>(deviceConfig, PRESSURE_SENSOR_LABEL);
  ASSERT_NE(nullptr, pressureSensor);
  EXPECT_EQ(daemonConfig.i2cBus(), pressureSensor->addr.bus);
  EXPECT_EQ(daemonConfig.pressureSensorI2cAddr(), pressureSensor->addr.dev);
  EXPECT_EQ(daemonConfig.gpioChip(), pressureSensor->signalPin.chip);
  EXPECT_EQ(daemonConfig.pressureSensorSignalPin(), pressureSensor->signalPin.pin);

  auto tempSensor =
    getComponent<libopenpresso::Max31856TemperatureSensorConfig>(deviceConfig, TEMPERATURE_SENSOR_LABEL);
  ASSERT_NE(nullptr, tempSensor);
  EXPECT_EQ(daemonConfig.temperatureSensorSpiAddr(), tempSensor->spiDev);
  EXPECT_EQ(daemonConfig.gpioChip(), tempSensor->signalPin.chip);
  EXPECT_EQ(daemonConfig.temperatureSensorSignalPin(), tempSensor->signalPin.pin);
  EXPECT_EQ(0, tempSensor->watchdogMinValidValue);
  EXPECT_EQ(daemonConfig.watchdogTemperatureLimit(), tempSensor->watchdogMaxValidValue);

  auto weightSensor =
    getComponent<libopenpresso::Nau7802WeightSensorConfig>(deviceConfig, WEIGHT_SENSOR_LABEL);
  ASSERT_NE(nullptr, weightSensor);
  EXPECT_EQ(daemonConfig.i2cBus(), weightSensor->addr.bus);
  EXPECT_EQ(daemonConfig.weightSensorI2cAddr(), weightSensor->addr.dev);
  EXPECT_EQ(daemonConfig.gpioChip(), weightSensor->signalPin.chip);
  EXPECT_EQ(daemonConfig.weightSensorSignalPin(), weightSensor->signalPin.pin);
  EXPECT_EQ(daemonConfig.weightSensorScale(), weightSensor->scale);
  EXPECT_EQ(daemonConfig.weightSensorFilterTiming(), weightSensor->flowRateSmoothingTime);

  auto acSensor = getComponent<libopenpresso::AcZeroCrossSensorConfig>(deviceConfig, AC_SENSOR_LABEL);
  ASSERT_NE(nullptr, acSensor);
  EXPECT_EQ(daemonConfig.gpioChip(), acSensor->signalPin.chip);
  EXPECT_EQ(daemonConfig.mainsZeroCrossPin(), acSensor->signalPin.pin);

  auto internalFlow =
    getComponent<libopenpresso::VibroPumpFlowSensor>(deviceConfig, INTERNAL_FLOW_SENSOR_LABEL);
  ASSERT_NE(nullptr, internalFlow);
  EXPECT_EQ(PRESSURE_SENSOR_LABEL, internalFlow->pressureSensor);
  EXPECT_EQ(PUMP_DRIVER_LABEL, internalFlow->pumpPulseController);
  EXPECT_EQ(daemonConfig.pumpVolumePerPulse(), internalFlow->volumePerPulse);
  EXPECT_EQ(daemonConfig.pumpStallPressure(), internalFlow->pumpStallPressure);

  // --- UI Elements ---

  auto powerBtn = getComponent<libopenpresso::LogicalInputPinConfig>(deviceConfig, POWER_BUTTON_LABEL);
  ASSERT_NE(nullptr, powerBtn);
  EXPECT_EQ(daemonConfig.gpioChip(), powerBtn->addr.chip);
  EXPECT_EQ(daemonConfig.powerButtonSignalPin(), powerBtn->addr.pin);
  EXPECT_EQ(daemonConfig.buttonsDebouncePeriod(), powerBtn->debouncePeriod);
  EXPECT_EQ(daemonConfig.buttonsPullMode(), powerBtn->pull);
  EXPECT_EQ(daemonConfig.buttonsLogicInverted(), powerBtn->inverted);

  auto powerLed = getComponent<libopenpresso::LogicalOutputPinConfig>(deviceConfig, POWER_LED_LABEL);
  ASSERT_NE(nullptr, powerLed);
  EXPECT_EQ(daemonConfig.gpioChip(), powerLed->addr.chip);
  EXPECT_EQ(daemonConfig.powerLedOutputPin(), powerLed->addr.pin);
  EXPECT_FALSE(powerLed->initState);
  EXPECT_EQ(daemonConfig.ledsLogicInverted(), powerLed->inverted);

  // --- Drivers ---

  auto pumpDriver =
    getComponent<libopenpresso::PulseControlledDeviceConfig>(deviceConfig, PUMP_DRIVER_LABEL);
  ASSERT_NE(nullptr, pumpDriver);
  EXPECT_EQ(daemonConfig.gpioChip(), pumpDriver->pulsePin.chip);
  EXPECT_EQ(daemonConfig.pumpOutputPin(), pumpDriver->pulsePin.pin);
  EXPECT_EQ(AC_SENSOR_LABEL, pumpDriver->acZeroCrossSensor);

  auto heaterDriver =
    getComponent<libopenpresso::PulseControlledDeviceConfig>(deviceConfig, HEATER_DRIVER_LABEL);
  ASSERT_NE(nullptr, heaterDriver);
  EXPECT_EQ(daemonConfig.gpioChip(), heaterDriver->pulsePin.chip);
  EXPECT_EQ(daemonConfig.heaterOutputPin(), heaterDriver->pulsePin.pin);
  EXPECT_EQ(AC_SENSOR_LABEL, heaterDriver->acZeroCrossSensor);

  auto valveDriver = getComponent<libopenpresso::LogicalOutputPinConfig>(deviceConfig, VALVE_DRIVER_LABEL);
  ASSERT_NE(nullptr, valveDriver);
  EXPECT_EQ(daemonConfig.gpioChip(), valveDriver->addr.chip);
  EXPECT_EQ(daemonConfig.valveOutputPin(), valveDriver->addr.pin);
  EXPECT_FALSE(valveDriver->initState);
  EXPECT_FALSE(valveDriver->inverted);

  // --- Controllers ---

  auto pumpPower =
    getComponent<libopenpresso::PulsePowerControllerConfig>(deviceConfig, PUMP_POWER_CONTROLLER_LABEL);
  ASSERT_NE(nullptr, pumpPower);
  EXPECT_EQ(PUMP_DRIVER_LABEL, pumpPower->pulseControlledDevice);
  EXPECT_EQ(PUMP_POWER_DUTY_CYCLE, pumpPower->dutyCycle);

  auto heaterPower =
    getComponent<libopenpresso::PulsePowerControllerConfig>(deviceConfig, HEATER_POWER_CONTROLLER_LABEL);
  ASSERT_NE(nullptr, heaterPower);
  EXPECT_EQ(HEATER_DRIVER_LABEL, heaterPower->pulseControlledDevice);
  EXPECT_EQ(HEATER_POWER_DUTY_CYCLE, heaterPower->dutyCycle);

  auto pressureCtrl =
    getComponent<libopenpresso::PulsePressureControllerConfig>(deviceConfig, PRESSURE_CONTROLLER_LABEL);
  ASSERT_NE(nullptr, pressureCtrl);
  EXPECT_EQ(PUMP_DRIVER_LABEL, pressureCtrl->pulseController);
  EXPECT_EQ(PRESSURE_SENSOR_LABEL, pressureCtrl->sensor);

  auto flowRateCtrl =
    getComponent<libopenpresso::IntegralFlowRateControllerConfig>(deviceConfig, FLOW_RATE_CONTROLLER_LABEL);
  ASSERT_NE(nullptr, flowRateCtrl);
  EXPECT_EQ(PUMP_POWER_CONTROLLER_LABEL, flowRateCtrl->powerController);
  EXPECT_EQ(WEIGHT_SENSOR_LABEL, flowRateCtrl->sensor);
  EXPECT_FLOAT_EQ(daemonConfig.flowControllerMultiplier(), flowRateCtrl->feedbackCoef);

  auto internalFlowRateCtrl =
    getComponent<libopenpresso::VibroPumpFlowController>(deviceConfig, INTERNAL_FLOW_RATE_CONTROLLER_LABEL);
  ASSERT_NE(nullptr, internalFlowRateCtrl);
  EXPECT_EQ(INTERNAL_FLOW_SENSOR_LABEL, internalFlowRateCtrl->pumpFlowSensor);
  EXPECT_EQ(daemonConfig.mainsFrequency(), internalFlowRateCtrl->mainsFrequency);

  // --- Temperature Controllers ---

  auto brewTempCtrl =
    getComponent<libopenpresso::TemperaturePidControllerConfig>(deviceConfig,
                                                                BREW_TEMPERATURE_CONTROLLER_LABEL);
  ASSERT_NE(nullptr, brewTempCtrl);
  EXPECT_EQ(HEATER_POWER_CONTROLLER_LABEL, brewTempCtrl->powerController);
  EXPECT_EQ(TEMPERATURE_SENSOR_LABEL, brewTempCtrl->sensor);
  EXPECT_EQ(INTERNAL_FLOW_SENSOR_LABEL, brewTempCtrl->flowCounter.value_or(""));
  EXPECT_FALSE(brewTempCtrl->enablePidStateDump);

  // Just check one field of pid settings to ensure it passed the struct
  EXPECT_FLOAT_EQ(daemonConfig.brewTemperatureControllerPidSettings().p, brewTempCtrl->pidSettings.p);

  auto steamPreheatCtrl = getComponent<libopenpresso::TemperaturePidControllerConfig>(
    deviceConfig, STEAM_PREHEAT_TEMPERATURE_CONTROLLER_LABEL);
  ASSERT_NE(nullptr, steamPreheatCtrl);
  EXPECT_FALSE(steamPreheatCtrl->flowCounter.has_value());
  EXPECT_FLOAT_EQ(daemonConfig.steamPreheatTemperatureControllerPidSettings().p,
                  steamPreheatCtrl->pidSettings.p);

  auto steamActiveCtrl = getComponent<libopenpresso::TemperaturePidControllerConfig>(
    deviceConfig, STEAM_ACTIVE_TEMPERATURE_CONTROLLER_LABEL);
  ASSERT_NE(nullptr, steamActiveCtrl);
  EXPECT_FALSE(steamActiveCtrl->flowCounter.has_value());
  EXPECT_FLOAT_EQ(daemonConfig.steamActiveTemperatureControllerPidSettings().p,
                  steamActiveCtrl->pidSettings.p);

  // --- High Level ---

  auto brewProfiler = getComponent<libopenpresso::BrewProfilerConfig>(deviceConfig, BREW_PROFILER_LABEL);
  ASSERT_NE(nullptr, brewProfiler);
  EXPECT_EQ(BREW_PROFILER_STATE_UPDATE_PERIOD, brewProfiler->updatePeriod);
  EXPECT_EQ(PRESSURE_CONTROLLER_LABEL, brewProfiler->pressureController);
  EXPECT_EQ(FLOW_RATE_CONTROLLER_LABEL, brewProfiler->flowController);
  EXPECT_EQ(VALVE_DRIVER_LABEL, brewProfiler->valveController);
  EXPECT_EQ(WEIGHT_SENSOR_LABEL, brewProfiler->weightSensor);

  auto steamCtrl =
    getComponent<libopenpresso::SteamControllerConfig>(deviceConfig, STEAM_CONTROLLER_LABEL);
  ASSERT_NE(nullptr, steamCtrl);
  EXPECT_EQ(daemonConfig.steamTemperature(), steamCtrl->steamTemperature);
  EXPECT_EQ(daemonConfig.steamPressureThreshold(), steamCtrl->pressureThreshold);
  EXPECT_EQ(daemonConfig.steamTemperature() - STEAM_TEMPERATURE_THRESHOLD_OFFSET,
            steamCtrl->temperatureThreshold);
  EXPECT_EQ(daemonConfig.steamRefillFlow(), steamCtrl->refillFlow);
  EXPECT_EQ(STEAM_CONTROLLER_STATE_UPDATE_PERIOD, steamCtrl->refillUpdatePeriod);
  EXPECT_EQ(STEAM_PREHEAT_TEMPERATURE_CONTROLLER_LABEL, steamCtrl->preheatController);
  EXPECT_EQ(STEAM_ACTIVE_TEMPERATURE_CONTROLLER_LABEL, steamCtrl->steamingTemperatureController);
  EXPECT_EQ(TEMPERATURE_SENSOR_LABEL, steamCtrl->temperatureSensor);
  EXPECT_EQ(PRESSURE_SENSOR_LABEL, steamCtrl->pressureSensor);
  EXPECT_EQ(INTERNAL_FLOW_RATE_CONTROLLER_LABEL, steamCtrl->flowRateController);
}

TEST_F(LibopenpressoConfigMakerTest, WatchdogEnabledMinConfig)
{
  // Default config has watchdog enabled
  auto daemonConfig = OpenpressodConfig::fromString(MINIMAL_CONFIG);
  LibopenpressoConfigMaker maker(daemonConfig, nullptr);
  auto deviceConfig = maker.make();

  ASSERT_TRUE(deviceConfig.watchdog.has_value());
  EXPECT_EQ(daemonConfig.watchdogDevice(), deviceConfig.watchdog->watchdogDev);
  EXPECT_EQ(daemonConfig.watchdogTimeout(), deviceConfig.watchdog->timeout);
}

#include "libopenpresso_config_maker.hpp"

#include "libopenpresso/component_config.hpp"
#include "libopenpresso/config.hpp"
#include "libopenpresso/types.hpp"
#include "libopenpresso_config_constants.hpp"
#include "libopenpresso_config_labels.hpp"
#include "openpressod_config.hpp"

#include <memory>
#include <optional>
#include <utility>

using namespace openpressod;
using namespace openpressod::libopenpresso_config_labels;
using namespace openpressod::libopenpresso_config_constants;

LibopenpressoConfigMaker::LibopenpressoConfigMaker(OpenpressodConfig daemonConfig,
                                                   std::shared_ptr<spdlog::logger> logger)
: m_daemonConfig{std::move(daemonConfig)}
, m_logger{std::move(logger)}
{
}

libopenpresso::DeviceConfig LibopenpressoConfigMaker::make()
{
  return {
    .components =
      {
        makePressureSensorConfig(),
        makeTemperatureSensorConfig(),
        makeWeightSensorConfig(),
        makeAcSensorConfig(),
        makeInternalFlowSensorConfig(),

        makeButtonConfig(POWER_BUTTON_LABEL, m_daemonConfig.powerButtonSignalPin()),
        makeButtonConfig(BREW_BUTTON_LABEL, m_daemonConfig.brewButtonSignalPin()),
        makeButtonConfig(STEAM_BUTTON_LABEL, m_daemonConfig.steamButtonSignalPin()),

        makeLedConfig(POWER_LED_LABEL, m_daemonConfig.powerLedOutputPin()),
        makeLedConfig(BREW_LED_LABEL, m_daemonConfig.brewLedOutputPin()),
        makeLedConfig(STEAM_LED_LABEL, m_daemonConfig.steamLedOutputPin()),

        makePumpDriverConfig(),
        makeHeaterDriverConfig(),
        makeValveDriverConfig(),

        makePumpPowerControllerConfig(),
        makeHeaterPowerControllerConfig(),

        makePressureControllerConfig(),
        makeFlowRateControllerConfig(),
        makeInternalFlowRateControllerConfig(),
        makeBrewTemperatureControllerConfig(),
        makeSteamPreheatTemperatureControllerConfig(),
        makeSteamActiveTemperatureControllerConfig(),

        makeBrewProfilerConfig(),
        makeSteamControllerConfig(),
      },
    .watchdog = makeWatchdogConfig(),
    .logger = m_logger,
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makePressureSensorConfig() const
{
  return {
    PRESSURE_SENSOR_LABEL,
    libopenpresso::Ads1115PressureSensorConfig{
      .addr =
        {
          .bus = m_daemonConfig.i2cBus(),
          .dev = m_daemonConfig.pressureSensorI2cAddr(),
        },
      .signalPin =
        {
          .chip = m_daemonConfig.gpioChip(),
          .pin = m_daemonConfig.pressureSensorSignalPin(),
        },
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeTemperatureSensorConfig() const
{
  return {
    TEMPERATURE_SENSOR_LABEL,
    libopenpresso::Max31856TemperatureSensorConfig{
      .spiDev = m_daemonConfig.temperatureSensorSpiAddr(),
      .signalPin =
        {
          .chip = m_daemonConfig.gpioChip(),
          .pin = m_daemonConfig.temperatureSensorSignalPin(),
        },
      .watchdogMinValidValue = 0,
      .watchdogMaxValidValue = m_daemonConfig.watchdogTemperatureLimit(),
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeWeightSensorConfig() const
{
  return {
    WEIGHT_SENSOR_LABEL,
    libopenpresso::Nau7802WeightSensorConfig{
      .addr =
        {
          .bus = m_daemonConfig.i2cBus(),
          .dev = m_daemonConfig.weightSensorI2cAddr(),
        },
      .signalPin =
        {
          .chip = m_daemonConfig.gpioChip(),
          .pin = m_daemonConfig.weightSensorSignalPin(),
        },
      .scale = m_daemonConfig.weightSensorScale(),
      .flowRateSmoothingTime = m_daemonConfig.weightSensorFilterTiming(),
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeAcSensorConfig() const
{
  return {
    AC_SENSOR_LABEL,
    libopenpresso::AcZeroCrossSensorConfig{
      .signalPin =
        {
          .chip = m_daemonConfig.gpioChip(),
          .pin = m_daemonConfig.mainsZeroCrossPin(),
        },
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeInternalFlowSensorConfig() const
{
  return {
    INTERNAL_FLOW_SENSOR_LABEL,
    libopenpresso::VibroPumpFlowSensor{
      .pressureSensor = PRESSURE_SENSOR_LABEL,
      .pumpPulseController = PUMP_DRIVER_LABEL,
      .volumePerPulse = m_daemonConfig.pumpVolumePerPulse(),
      .pumpStallPressure = m_daemonConfig.pumpStallPressure(),
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeButtonConfig(
  const char* label, libopenpresso::pin_number_t pin) const
{
  return {
    label,
    libopenpresso::LogicalInputPinConfig{
      .addr =
        {
          .chip = m_daemonConfig.gpioChip(),
          .pin = pin,
        },
      .debouncePeriod = m_daemonConfig.buttonsDebouncePeriod(),
      .pull = m_daemonConfig.buttonsPullMode(),
      .inverted = m_daemonConfig.buttonsLogicInverted(),
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeLedConfig(
  const char* label, libopenpresso::pin_number_t pin) const
{
  return {
    label,
    libopenpresso::LogicalOutputPinConfig{
      .addr =
        {
          .chip = m_daemonConfig.gpioChip(),
          .pin = pin,
        },
      .initState = false,
      .inverted = m_daemonConfig.ledsLogicInverted(),
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makePumpDriverConfig() const
{
  return {
    PUMP_DRIVER_LABEL,
    libopenpresso::PulseControlledDeviceConfig{
      .pulsePin =
        {
          .chip = m_daemonConfig.gpioChip(),
          .pin = m_daemonConfig.pumpOutputPin(),
        },
      .acZeroCrossSensor = AC_SENSOR_LABEL,
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeHeaterDriverConfig() const
{
  return {
    HEATER_DRIVER_LABEL,
    libopenpresso::PulseControlledDeviceConfig{
      .pulsePin =
        {
          .chip = m_daemonConfig.gpioChip(),
          .pin = m_daemonConfig.heaterOutputPin(),
        },
      .acZeroCrossSensor = AC_SENSOR_LABEL,
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeValveDriverConfig() const
{
  return {
    VALVE_DRIVER_LABEL,
    libopenpresso::LogicalOutputPinConfig{
      .addr =
        {
          .chip = m_daemonConfig.gpioChip(),
          .pin = m_daemonConfig.valveOutputPin(),
        },
      .initState = false,
      .inverted = false,
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makePumpPowerControllerConfig()
{
  return {
    PUMP_POWER_CONTROLLER_LABEL,
    libopenpresso::PulsePowerControllerConfig{
      .pulseControlledDevice = PUMP_DRIVER_LABEL,
      .dutyCycle = PUMP_POWER_DUTY_CYCLE,
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeHeaterPowerControllerConfig()
{
  return {
    HEATER_POWER_CONTROLLER_LABEL,
    libopenpresso::PulsePowerControllerConfig{
      .pulseControlledDevice = HEATER_DRIVER_LABEL,
      .dutyCycle = HEATER_POWER_DUTY_CYCLE,
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makePressureControllerConfig()
{
  return {
    PRESSURE_CONTROLLER_LABEL,
    libopenpresso::PulsePressureControllerConfig{
      .pulseController = PUMP_DRIVER_LABEL,
      .sensor = PRESSURE_SENSOR_LABEL,
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeFlowRateControllerConfig() const
{
  return {
    FLOW_RATE_CONTROLLER_LABEL,
    libopenpresso::IntegralFlowRateControllerConfig{
      .powerController = PUMP_POWER_CONTROLLER_LABEL,
      .sensor = WEIGHT_SENSOR_LABEL,
      .feedbackCoef = m_daemonConfig.flowControllerMultiplier(),
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeInternalFlowRateControllerConfig() const
{
  return {
    INTERNAL_FLOW_RATE_CONTROLLER_LABEL,
    libopenpresso::VibroPumpFlowController{
      .pumpFlowSensor = INTERNAL_FLOW_SENSOR_LABEL,
      .mainsFrequency = m_daemonConfig.mainsFrequency(),
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeBrewTemperatureControllerConfig() const
{
  return {
    BREW_TEMPERATURE_CONTROLLER_LABEL,
    libopenpresso::TemperaturePidControllerConfig{
      .powerController = HEATER_POWER_CONTROLLER_LABEL,
      .sensor = TEMPERATURE_SENSOR_LABEL,
      .pidSettings = m_daemonConfig.brewTemperatureControllerPidSettings(),
      .enablePidStateDump = m_daemonConfig.pidStateMonitoringEnabled(),
      .flowCounter = INTERNAL_FLOW_SENSOR_LABEL,
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeSteamPreheatTemperatureControllerConfig() const
{
  return {
    STEAM_PREHEAT_TEMPERATURE_CONTROLLER_LABEL,
    libopenpresso::TemperaturePidControllerConfig{
      .powerController = HEATER_POWER_CONTROLLER_LABEL,
      .sensor = TEMPERATURE_SENSOR_LABEL,
      .pidSettings = m_daemonConfig.steamPreheatTemperatureControllerPidSettings(),
      .enablePidStateDump = m_daemonConfig.pidStateMonitoringEnabled(),
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeSteamActiveTemperatureControllerConfig() const
{
  return {
    STEAM_ACTIVE_TEMPERATURE_CONTROLLER_LABEL,
    libopenpresso::TemperaturePidControllerConfig{
      .powerController = HEATER_POWER_CONTROLLER_LABEL,
      .sensor = TEMPERATURE_SENSOR_LABEL,
      .pidSettings = m_daemonConfig.steamActiveTemperatureControllerPidSettings(),
      .enablePidStateDump = m_daemonConfig.pidStateMonitoringEnabled(),
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeBrewProfilerConfig()
{
  return {
    BREW_PROFILER_LABEL,
    libopenpresso::BrewProfilerConfig{
      .updatePeriod = BREW_PROFILER_STATE_UPDATE_PERIOD,
      .pressureController = PRESSURE_CONTROLLER_LABEL,
      .flowController = FLOW_RATE_CONTROLLER_LABEL,
      .valveController = VALVE_DRIVER_LABEL,
      .weightSensor = WEIGHT_SENSOR_LABEL,
    },
  };
}

LibopenpressoConfigMaker::config_entry_t LibopenpressoConfigMaker::makeSteamControllerConfig() const
{
  return {
    STEAM_CONTROLLER_LABEL,
    libopenpresso::SteamControllerConfig{
      .pressureThreshold = m_daemonConfig.steamPressureThreshold(),
      .temperatureRelativeThreshold = STEAM_TEMPERATURE_THRESHOLD_OFFSET,
      .refillFlow = m_daemonConfig.steamRefillFlow(),
      .refillUpdatePeriod = STEAM_CONTROLLER_STATE_UPDATE_PERIOD,
      .preheatController = STEAM_PREHEAT_TEMPERATURE_CONTROLLER_LABEL,
      .steamingTemperatureController = STEAM_ACTIVE_TEMPERATURE_CONTROLLER_LABEL,
      .temperatureSensor = TEMPERATURE_SENSOR_LABEL,
      .pressureSensor = PRESSURE_SENSOR_LABEL,
      .flowRateController = INTERNAL_FLOW_RATE_CONTROLLER_LABEL,
    },
  };
}

std::optional<libopenpresso::WatchdogConfig> LibopenpressoConfigMaker::makeWatchdogConfig() const
{
  if (m_daemonConfig.watchdogEnabled()) {
    return libopenpresso::WatchdogConfig{
      .watchdogDev = m_daemonConfig.watchdogDevice(),
      .timeout = m_daemonConfig.watchdogTimeout(),
    };
  }
  return std::nullopt;
}

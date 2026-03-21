#ifndef OPENPRESSO_CONFIG_HPP
#define OPENPRESSO_CONFIG_HPP

#include <chrono>
#include <filesystem>
#include <toml.hpp>

#include <libopenpresso/config.hpp>
#include <libopenpresso/i2c_info.hpp>
#include <libopenpresso/types.hpp>
#include <spdlog/common.h>

using namespace std::chrono_literals;

namespace openpressod
{

class OpenpressodConfig {
  static constexpr auto DEFAULT_DAEMON_FOLDER_NAME = ".openpressod";

public:
  static std::filesystem::path daemonBaseDirectory();
  static OpenpressodConfig fromFile(const std::filesystem::path& configPath = daemonBaseDirectory() /
                                                                              "config.toml");
  static OpenpressodConfig fromString(const std::string& conf);

  // [global]
  std::filesystem::path brewProfilePath() const;
  std::filesystem::path socketPath() const;
  size_t mainsFrequency() const;
  libopenpresso::unix_dev_addr_t i2cBus() const;
  libopenpresso::unix_dev_addr_t gpioChip() const;
  libopenpresso::pin_number_t mainsZeroCrossPin() const;
  libopenpresso::pin_number_t valveOutputPin() const;

  // [logs]
  bool logfileOutputEnabled() const;
  spdlog::level::level_enum logFileLevel() const;
  std::string logFileOutputPattern() const;
  std::filesystem::path logFilesDirectory() const;
  std::string consoleOutputPattern() const;
  spdlog::level::level_enum consoleOutputLevel() const;

  // [watchdog]
  bool watchdogEnabled() const;
  libopenpresso::unix_dev_addr_t watchdogDevice() const;
  libopenpresso::time_delta_t watchdogTimeout() const;
  libopenpresso::millidegrees_t watchdogTemperatureLimit() const;

  //[buttons]
  bool buttonsLogicInverted() const;
  bool powerButtonBistable() const;
  bool brewButtonBistable() const;
  bool steamButtonBistable() const;
  libopenpresso::PinPull buttonsPullMode() const;
  libopenpresso::time_delta_t buttonsDebouncePeriod() const;
  libopenpresso::pin_number_t powerButtonSignalPin() const;
  libopenpresso::pin_number_t brewButtonSignalPin() const;
  libopenpresso::pin_number_t steamButtonSignalPin() const;

  // [leds]
  bool ledsLogicInverted() const;
  libopenpresso::pin_number_t powerLedOutputPin() const;
  libopenpresso::pin_number_t brewLedOutputPin() const;
  libopenpresso::pin_number_t steamLedOutputPin() const;

  // [pressure_sensor]
  libopenpresso::pin_number_t pressureSensorSignalPin() const;
  libopenpresso::i2c_dev_addr_t pressureSensorI2cAddr() const;

  // [weight_sensor]
  uint32_t weightSensorScale() const;
  libopenpresso::time_delta_t weightSensorFilterTiming() const;
  libopenpresso::pin_number_t weightSensorSignalPin() const;
  libopenpresso::i2c_dev_addr_t weightSensorI2cAddr() const;

  // [temperature_sensor]
  libopenpresso::time_delta_t temperatureSensorFilterTiming() const;
  libopenpresso::unix_dev_addr_t temperatureSensorSpiAddr() const;
  libopenpresso::pin_number_t temperatureSensorSignalPin() const;

  // [pump]
  float flowControllerMultiplier() const;
  libopenpresso::pin_number_t pumpOutputPin() const;
  libopenpresso::micrograms_t pumpVolumePerPulse() const;
  libopenpresso::millibars_t pumpStallPressure() const;

  // [heater]
  libopenpresso::pin_number_t heaterOutputPin() const;
  // [heater.brew_pid_settings]
  libopenpresso::PidSettings brewTemperatureControllerPidSettings() const;

  // [steam]
  libopenpresso::millibars_t steamPressureThreshold() const;
  libopenpresso::millidegrees_t steamTemperature() const;
  libopenpresso::milligrams_p_second_t steamRefillFlow() const;
  // [steam.preheat_pid_settings]
  libopenpresso::PidSettings steamPreheatTemperatureControllerPidSettings() const;
  // [steam.active_pid_settings]
  libopenpresso::PidSettings steamActiveTemperatureControllerPidSettings() const;

private:
  OpenpressodConfig(toml::value config);

private:
  toml::value m_config;
};

} // namespace openpressod

#endif // OPENPRESSO_CONFIG_HPP
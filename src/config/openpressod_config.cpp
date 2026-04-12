#include "openpressod_config.hpp"

#include "config_defaults.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <pwd.h>
#include <string>
#include <toml.hpp>
#include <unistd.h>
#include <utility>

#include <libopenpresso/pid_settings.hpp>
#include <libopenpresso/pin_info.hpp>
#include <libopenpresso/types.hpp>
#include <magic_enum/magic_enum.hpp>
#include <spdlog/common.h>

using namespace openpressod;
using namespace openpressod::config_defaults;

OpenpressodConfig::OpenpressodConfig(toml::value config)
: m_config{std::move(config)}
{
}

OpenpressodConfig OpenpressodConfig::fromFile(const std::filesystem::path& configPath)
{
  return {toml::parse(configPath)};
}

OpenpressodConfig openpressod::OpenpressodConfig::fromString(const std::string& conf)
{
  return {toml::parse_str(conf)};
}

std::filesystem::path OpenpressodConfig::brewProfilePath() const
{
  return toml::find_or(m_config,
                       "global",
                       "brew_profile_path",
                       (daemonBaseDirectory() / DEFAULT_BREW_PROFILE_NAME).c_str());
}

std::filesystem::path OpenpressodConfig::socketPath() const
{
  return toml::find_or(m_config, "global", "socket_path", DEFAULT_SOCKET_PATH);
}

size_t OpenpressodConfig::mainsFrequency() const
{
  return toml::find<size_t>(m_config, "global", "mains_frequency");
}

libopenpresso::unix_dev_addr_t OpenpressodConfig::i2cBus() const
{
  return toml::find_or(m_config, "global", "i2c_bus", DEFAULT_I2C_BUS);
}

libopenpresso::unix_dev_addr_t OpenpressodConfig::gpioChip() const
{
  return toml::find_or(m_config, "global", "gpio_chip", DEFAULT_GPIO_CHIP);
}

libopenpresso::pin_number_t OpenpressodConfig::mainsZeroCrossPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "global", "mains_zc_pin");
}

libopenpresso::pin_number_t OpenpressodConfig::valveOutputPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "valve", "control_pin");
}

bool OpenpressodConfig::logfileOutputEnabled() const
{
  return toml::find_or(m_config, "log", "file_output_enabled", DEFAULT_LOGFILE_ENABLE);
}

spdlog::level::level_enum OpenpressodConfig::logFileLevel() const
{
  return magic_enum::enum_cast<spdlog::level::level_enum>(
           toml::find_or(m_config, "log", "file_loglevel", magic_enum::enum_name(DEFAULT_LOGFILE_LEVEL)))
    .value();
}

std::string OpenpressodConfig::logFileOutputPattern() const
{
  return toml::find_or(m_config, "log", "file_output_pattern", DEFAULT_LOGFILE_OUTPUT_PATTERN);
}

std::filesystem::path OpenpressodConfig::logFilesDirectory() const
{
  return toml::find_or(m_config,
                       "log",
                       "files_directory",
                       (daemonBaseDirectory() / DEFAULT_LOGFILES_DIRECTORY_NAME).c_str());
}

std::string OpenpressodConfig::consoleOutputPattern() const
{
  return toml::find_or(m_config, "log", "console_output_pattern", DEFAULT_CONSOLE_LOG_OUTPUT_PATTERN);
}

spdlog::level::level_enum OpenpressodConfig::consoleOutputLevel() const
{
  return magic_enum::enum_cast<spdlog::level::level_enum>(
           toml::find_or(m_config, "log", "console_loglevel", magic_enum::enum_name(DEFAULT_CONSOLE_LOG_LEVEL)))
    .value();
}

bool OpenpressodConfig::watchdogEnabled() const
{
  return toml::find_or(m_config, "watchdog", "enabled", DEFAULT_WATCHDOG_ENABLE);
}

libopenpresso::unix_dev_addr_t OpenpressodConfig::watchdogDevice() const
{
  return toml::find_or(m_config, "watchdog", "device_path", DEFAULT_WATCHDOG);
}

libopenpresso::time_delta_t OpenpressodConfig::watchdogTimeout() const
{
  return toml::find_or(m_config, "watchdog", "timeout", DEFAULT_WATCHDOG_TIMEOUT);
}

libopenpresso::millidegrees_t OpenpressodConfig::watchdogTemperatureLimit() const
{
  return toml::find_or(m_config, "watchdog", "temperature_limit", DEFAULT_WATCHDOG_RESET_TEMPERATURE);
}

bool OpenpressodConfig::buttonsLogicInverted() const
{
  return toml::find_or(m_config, "buttons", "inverted", DEFAULT_BUTTONS_LOGIC_INVERTED);
}

bool OpenpressodConfig::powerButtonBistable() const
{
  return toml::find_or(m_config, "buttons", "power_bistable", DEFAULT_POWER_BUTTON_BISTABLE);
}

bool OpenpressodConfig::brewButtonBistable() const
{
  return toml::find_or(m_config, "buttons", "brew_bistable", DEFAULT_BREW_BUTTON_BISTABLE);
}

bool OpenpressodConfig::steamButtonBistable() const
{
  return toml::find_or(m_config, "buttons", "steam_bistable", DEFAULT_STEAM_BUTTON_BISTABLE);
}

libopenpresso::PinPull OpenpressodConfig::buttonsPullMode() const
{
  return magic_enum::enum_cast<libopenpresso::PinPull>(
           toml::find_or(m_config, "buttons", "pull_mode", magic_enum::enum_name(DEFAULT_BUTTONS_PULL_MODE)))
    .value();
}

libopenpresso::time_delta_t OpenpressodConfig::buttonsDebouncePeriod() const
{
  return toml::find_or(m_config, "buttons", "debounce_period", DEFAULT_BUTTON_DEBOUNCE_PERIOD);
}

libopenpresso::pin_number_t OpenpressodConfig::powerButtonSignalPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "buttons", "power_pin");
}

libopenpresso::pin_number_t OpenpressodConfig::brewButtonSignalPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "buttons", "brew_pin");
}

libopenpresso::pin_number_t OpenpressodConfig::steamButtonSignalPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "buttons", "steam_pin");
}

bool OpenpressodConfig::ledsLogicInverted() const
{
  return toml::find_or(m_config, "leds", "inverted", DEFAULT_LEDS_LOGIC_INVERTED);
}

libopenpresso::pin_number_t OpenpressodConfig::powerLedOutputPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "leds", "power_pin");
}

libopenpresso::pin_number_t OpenpressodConfig::brewLedOutputPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "leds", "brew_pin");
}

libopenpresso::pin_number_t OpenpressodConfig::steamLedOutputPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "leds", "steam_pin");
}

libopenpresso::pin_number_t OpenpressodConfig::pressureSensorSignalPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "pressure_sensor", "signal_pin");
}

libopenpresso::i2c_dev_addr_t OpenpressodConfig::pressureSensorI2cAddr() const
{
  return toml::find_or(m_config, "pressure_sensor", "i2c_addr", DEFAULT_ADS1115_ADDR);
}

uint32_t OpenpressodConfig::weightSensorScale() const
{
  return toml::find<uint32_t>(m_config, "weight_sensor", "scale");
}

libopenpresso::time_delta_t OpenpressodConfig::weightSensorFilterTiming() const
{
  return toml::find_or(m_config, "weight_sensor", "filter_timing", DEFAULT_WEIGHT_D_FILTER_SMOOTHING_PERIOD);
}

libopenpresso::pin_number_t OpenpressodConfig::weightSensorSignalPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "weight_sensor", "signal_pin");
}

libopenpresso::i2c_dev_addr_t OpenpressodConfig::weightSensorI2cAddr() const
{
  return toml::find_or(m_config, "weight_sensor", "i2c_addr", DEFAULT_NAU7802_ADDR);
}

libopenpresso::time_delta_t OpenpressodConfig::temperatureSensorFilterTiming() const
{
  return toml::find_or(
    m_config, "temperature_sensor", "filter_timing", DEFAULT_TEMPERATURE_D_FILTER_SMOOTHING_PERIOD);
}

libopenpresso::unix_dev_addr_t OpenpressodConfig::temperatureSensorSpiAddr() const
{
  return toml::find_or(m_config, "temperature_sensor", "spi_addr", DEFAULT_TEMPERATURE_SENSOR_SPI);
}

libopenpresso::pin_number_t OpenpressodConfig::temperatureSensorSignalPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "temperature_sensor", "signal_pin");
}

float OpenpressodConfig::flowControllerMultiplier() const
{
  return toml::find<float>(m_config, "pump", "flow_controller_multiplier");
}

libopenpresso::pin_number_t OpenpressodConfig::pumpOutputPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "pump", "control_pin");
}

libopenpresso::micrograms_t OpenpressodConfig::pumpVolumePerPulse() const
{
  return toml::find<libopenpresso::micrograms_t>(m_config, "pump", "volume_per_pulse");
}

libopenpresso::millibars_t OpenpressodConfig::pumpStallPressure() const
{
  return toml::find<libopenpresso::millibars_t>(m_config, "pump", "stall_pressure");
}

libopenpresso::pin_number_t OpenpressodConfig::heaterOutputPin() const
{
  return toml::find<libopenpresso::pin_number_t>(m_config, "heater", "control_pin");
}

libopenpresso::millidegrees_t openpressod::OpenpressodConfig::brewTemperature() const
{
  return toml::find_or(m_config, "brew", "temperature", DEFAULT_BREW_TEMPERATURE);
}

libopenpresso::millibars_t openpressod::OpenpressodConfig::brewPressure() const
{
  return toml::find_or(m_config, "brew", "pressure", DEFAULT_BREW_PRESSURE);
}

libopenpresso::millidegrees_t OpenpressodConfig::brewTemperatureWindow() const
{
  return toml::find_or(m_config, "brew", "brew_temperature_window", DEFAULT_BREW_TEMPERATURE_WINDOW);
}

libopenpresso::PidSettings OpenpressodConfig::brewTemperatureControllerPidSettings() const
{
  return libopenpresso::PidSettings{
    .p = toml::find<libopenpresso::pid_coeffs_t>(m_config, "brew", "pid_settings", "p"),
    .d = toml::find<libopenpresso::pid_coeffs_t>(m_config, "brew", "pid_settings", "d"),
    .dTermRelax = toml::find<libopenpresso::pid_coeffs_t>(m_config, "brew", "pid_settings", "d_relax"),
    .dFilterTime = temperatureSensorFilterTiming(),
    .i = toml::find<libopenpresso::pid_coeffs_t>(m_config, "brew", "pid_settings", "i"),
    .iTermRelax = toml::find<libopenpresso::pid_coeffs_t>(m_config, "brew", "pid_settings", "i_relax"),
    .f = toml::find<libopenpresso::pid_coeffs_t>(m_config, "brew", "pid_settings", "f"),
    .w = toml::find<libopenpresso::pid_coeffs_t>(m_config, "brew", "pid_settings", "w"),
    .wDecay = toml::find<libopenpresso::pid_coeffs_t>(m_config, "brew", "pid_settings", "w_decay"),
  };
}

libopenpresso::millibars_t OpenpressodConfig::steamPressureThreshold() const
{
  return toml::find_or(m_config, "steam", "pressure_threshold", DEFAULT_STEAM_PRESSURE_THRESHOLD);
}

libopenpresso::millidegrees_t OpenpressodConfig::steamTemperature() const
{
  return toml::find_or(m_config, "steam", "temperature", DEFAULT_STEAM_TEMPERATURE);
}

libopenpresso::milligrams_p_second_t OpenpressodConfig::steamRefillFlow() const
{
  return toml::find_or(m_config, "steam", "refill_flow", DEFAULT_STEAM_REFILL_FLOW);
}

libopenpresso::millidegrees_t OpenpressodConfig::steamTemperatureWindow() const
{
  return toml::find_or(m_config, "steam", "steam_temperature_window", DEFAULT_STEAM_TEMPERATURE_WINDOW);
}

libopenpresso::PidSettings OpenpressodConfig::steamPreheatTemperatureControllerPidSettings() const
{
  return libopenpresso::PidSettings{
    .p = toml::find<libopenpresso::pid_coeffs_t>(m_config, "steam", "preheat_pid_settings", "p"),
    .d = toml::find<libopenpresso::pid_coeffs_t>(m_config, "steam", "preheat_pid_settings", "d"),
    .dTermRelax =
      toml::find<libopenpresso::pid_coeffs_t>(m_config, "steam", "preheat_pid_settings", "d_relax"),
    .dFilterTime = temperatureSensorFilterTiming(),
    .i = toml::find<libopenpresso::pid_coeffs_t>(m_config, "steam", "preheat_pid_settings", "i"),
    .iTermRelax =
      toml::find<libopenpresso::pid_coeffs_t>(m_config, "steam", "preheat_pid_settings", "i_relax"),
    .f = 0,
    .w = 0,
    .wDecay = 0,
  };
}

libopenpresso::PidSettings OpenpressodConfig::steamActiveTemperatureControllerPidSettings() const
{
  return libopenpresso::PidSettings{
    .p = toml::find<libopenpresso::pid_coeffs_t>(m_config, "steam", "active_pid_settings", "p"),
    .d = toml::find<libopenpresso::pid_coeffs_t>(m_config, "steam", "active_pid_settings", "d"),
    .dTermRelax =
      toml::find<libopenpresso::pid_coeffs_t>(m_config, "steam", "active_pid_settings", "d_relax"),
    .dFilterTime = temperatureSensorFilterTiming(),
    .i = toml::find<libopenpresso::pid_coeffs_t>(m_config, "steam", "active_pid_settings", "i"),
    .iTermRelax =
      toml::find<libopenpresso::pid_coeffs_t>(m_config, "steam", "active_pid_settings", "i_relax"),
    .f = 0,
    .w = 0,
    .wDecay = 0,
  };
}

std::filesystem::path OpenpressodConfig::daemonBaseDirectory()
{
  auto* pswd = getpwuid(getuid());
  if (pswd == nullptr) {
    throw std::runtime_error{"cannot get current user directory"};
  }
  return std::filesystem::path(pswd->pw_dir) / DEFAULT_DAEMON_FOLDER_NAME;
}

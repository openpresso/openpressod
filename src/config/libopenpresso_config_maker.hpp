#ifndef LIBOPENPRESSO_CONFIG_MAKER_HPP
#define LIBOPENPRESSO_CONFIG_MAKER_HPP

#include "openpressod_config.hpp"

#include <memory>
#include <optional>
#include <utility>

#include <libopenpresso/component_config.hpp>
#include <libopenpresso/config.hpp>
#include <libopenpresso/types.hpp>

namespace spdlog
{
class logger;
} // namespace spdlog

namespace openpressod
{

class LibopenpressoConfigMaker {
  using config_entry_t = std::pair<libopenpresso::component_label_t, libopenpresso::component_config_t>;

public:
  LibopenpressoConfigMaker(OpenpressodConfig daemonConfig, std::shared_ptr<spdlog::logger> logger);
  libopenpresso::DeviceConfig make();

private:
  config_entry_t makePressureSensorConfig() const;
  config_entry_t makeTemperatureSensorConfig() const;
  config_entry_t makeWeightSensorConfig() const;
  config_entry_t makeAcSensorConfig() const;
  config_entry_t makeInternalFlowSensorConfig() const;

  config_entry_t makeButtonConfig(const char* label, libopenpresso::pin_number_t pin) const;
  config_entry_t makeLedConfig(const char* label, libopenpresso::pin_number_t pin) const;

  config_entry_t makePumpDriverConfig() const;
  config_entry_t makeHeaterDriverConfig() const;
  config_entry_t makeValveDriverConfig() const;

  static config_entry_t makePumpPowerControllerConfig();
  static config_entry_t makeHeaterPowerControllerConfig();

  static config_entry_t makePressureControllerConfig();
  config_entry_t makeFlowRateControllerConfig() const;
  config_entry_t makeInternalFlowRateControllerConfig() const;
  config_entry_t makeBrewTemperatureControllerConfig() const;
  config_entry_t makeSteamPreheatTemperatureControllerConfig() const;
  config_entry_t makeSteamActiveTemperatureControllerConfig() const;

  static config_entry_t makeBrewProfilerConfig();
  config_entry_t makeSteamControllerConfig() const;

  std::optional<libopenpresso::WatchdogConfig> makeWatchdogConfig() const;

private:
  OpenpressodConfig m_daemonConfig;
  std::shared_ptr<spdlog::logger> m_logger;
};

} // namespace openpressod

#endif // LIBOPENPRESSO_CONFIG_MAKER_HPP
#include "leds_handler.hpp"

#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"

#include <cmath>
#include <optional>
#include <utility>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/types.hpp>
#include <spdlog/spdlog.h>

using namespace openpressod;

LedsHandler::LedsHandler(const libopenpresso::CorePtr& core, const OpenpressodConfig& config)
: m_brewTempretureWindow(config.brewTemperatureWindow())
, m_steamTempretureWindow(config.steamTemperatureWindow())
, m_power(core->getLogicalOutput(libopenpresso_config_labels::POWER_LED_LABEL))
, m_brew(core->getLogicalOutput(libopenpresso_config_labels::BREW_LED_LABEL))
, m_steam(core->getLogicalOutput(libopenpresso_config_labels::STEAM_LED_LABEL))
, m_temperatureSensor(core->getTemperatureSensor(libopenpresso_config_labels::TEMPERATURE_SENSOR_LABEL))
{
  m_power->activate();
  m_brew->activate();
  m_steam->activate();

  spdlog::debug("Leds manager created");
}

LedsHandler::~LedsHandler() // NOLINT (bugprone-exception-escape)
{
  if (m_temperatureCallback.has_value()) {
    m_temperatureSensor->unregisterCallback(std::exchange(m_temperatureCallback, std::nullopt).value());
  }
  m_power->deactivate();
  m_brew->deactivate();
  m_steam->deactivate();
}

void openpressod::LedsHandler::indicatePowerOff()
{
  if (m_temperatureCallback.has_value()) {
    m_temperatureSensor->unregisterCallback(std::exchange(m_temperatureCallback, std::nullopt).value());
  }
  m_power->setState(false);
  m_brew->setState(false);
  m_steam->setState(false);
}

void openpressod::LedsHandler::indicateBrewState(libopenpresso::millidegrees_t targetTemp)
{
  if (m_temperatureCallback.has_value()) {
    m_temperatureSensor->unregisterCallback(std::exchange(m_temperatureCallback, {}).value());
  }

  m_temperatureCallback = m_temperatureSensor->registerCallback(
    [targetTemp, window = m_brewTempretureWindow, led = m_brew](libopenpresso::millidegrees_t temp) {
      led->setState(std::abs(targetTemp - temp) <= window);
    });

  m_power->setState(true);
  m_steam->setState(false);
}

void openpressod::LedsHandler::indicateSteamState(libopenpresso::millidegrees_t targetTemp)
{
  if (m_temperatureCallback.has_value()) {
    m_temperatureSensor->unregisterCallback(std::exchange(m_temperatureCallback, {}).value());
  }

  m_temperatureCallback = m_temperatureSensor->registerCallback(
    [targetTemp, window = m_steamTempretureWindow, led = m_steam](libopenpresso::millidegrees_t temp) {
      led->setState(std::abs(targetTemp - temp) <= window);
    });

  m_power->setState(true);
  m_brew->setState(false);
}

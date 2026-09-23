#include "status_manager.hpp"

#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"
#include "leds/leds_handler.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/types.hpp>
#include <openpresso_proto/openpresso.pb.h>

using namespace openpressod;

StatusManager::StatusManager(const libopenpresso::CorePtr& core,
                             const OpenpressodConfig& config,
                             std::unique_ptr<LedsHandler>&& leds)
: m_brewTempretureWindow{config.brewTemperatureWindow()}
, m_steamTempretureWindow{config.steamTemperatureWindow()}
, m_leds{std::move(leds)}
, m_temperatureSensor{core->getTemperatureSensor(libopenpresso_config_labels::TEMPERATURE_SENSOR_LABEL)}
{
}

StatusManager::~StatusManager() // NOLINT (bugprone-exception-escape)
{
  if (m_temperatureCallback.has_value()) {
    m_temperatureSensor->unregisterCallback(std::exchange(m_temperatureCallback, std::nullopt).value());
  }
}

HeatingStatus StatusManager::getCurrentStatus() const noexcept
{
  return m_heatingStatus.load(std::memory_order_relaxed);
}

void StatusManager::disable()
{
  if (m_temperatureCallback.has_value()) {
    m_temperatureSensor->unregisterCallback(std::exchange(m_temperatureCallback, std::nullopt).value());
  }

  m_leds->indicatePowerOff();

  if (m_heatingStatusCached != HeatingStatus::HeatingStatus_DISABLED) {
    m_heatingStatusCached = HeatingStatus::HeatingStatus_DISABLED;
    m_heatingStatus.store(m_heatingStatusCached, std::memory_order_relaxed);
    std::invoke(m_callback, m_heatingStatusCached);
  }
}

void StatusManager::indicateBrewState(libopenpresso::millidegrees_t targetTemp)
{
  if (m_temperatureCallback.has_value()) {
    m_temperatureSensor->unregisterCallback(std::exchange(m_temperatureCallback, std::nullopt).value());
  }

  m_temperatureCallback =
    m_temperatureSensor->registerCallback([targetTemp, this](libopenpresso::millidegrees_t temp) {
      auto newStatus = m_heatingStatusCached;
      if (temp + m_brewTempretureWindow < targetTemp) {
        m_leds->indicateBrewPreheat();
        newStatus = HeatingStatus::HeatingStatus_Preheat;
      }
      else if (temp > targetTemp + m_brewTempretureWindow) {
        m_leds->indicateBrewOverheat();
        newStatus = HeatingStatus::HeatingStatus_Overheat;
      }
      else {
        m_leds->indicateBrewReady();
        newStatus = HeatingStatus::HeatingStatus_Ready;
      }

      if (newStatus != m_heatingStatusCached) {
        m_heatingStatusCached = newStatus;
        m_heatingStatus.store(newStatus, std::memory_order_relaxed);
        std::invoke(m_callback, newStatus);
      }
    });
}

void StatusManager::indicateSteamState(libopenpresso::millidegrees_t targetTemp)
{
  if (m_temperatureCallback.has_value()) {
    m_temperatureSensor->unregisterCallback(std::exchange(m_temperatureCallback, std::nullopt).value());
  }

  m_temperatureCallback =
    m_temperatureSensor->registerCallback([targetTemp, this](libopenpresso::millidegrees_t temp) {
      auto newStatus = m_heatingStatusCached;
      if (temp + m_steamTempretureWindow < targetTemp) {
        m_leds->indicateSteamPreheat();
        newStatus = HeatingStatus::HeatingStatus_Preheat;
      }
      else if (temp > targetTemp + m_steamTempretureWindow) {
        m_leds->indicateSteamOverheat();
        newStatus = HeatingStatus::HeatingStatus_Overheat;
      }
      else {
        m_leds->indicateSteamReady();
        newStatus = HeatingStatus::HeatingStatus_Ready;
      }

      if (newStatus != m_heatingStatusCached) {
        m_heatingStatusCached = newStatus;
        m_heatingStatus.store(newStatus, std::memory_order_relaxed);
        std::invoke(m_callback, newStatus);
      }
    });
}

void StatusManager::setCallback(std::function<void(HeatingStatus)> callback)
{
  m_callback = std::move(callback);
}

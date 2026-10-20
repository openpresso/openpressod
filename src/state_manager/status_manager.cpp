#include "status_manager.hpp"

#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"
#include "leds/leds_handler.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/interfaces/temperature_sensor.hpp>
#include <libopenpresso/types.hpp>
#include <openpresso_proto/openpresso.pb.h>

using namespace openpressod;

StatusManager::StatusManager(const libopenpresso::CorePtr& core,
                             const OpenpressodConfig& config,
                             std::unique_ptr<LedsHandler>&& leds)
: m_brewTemperatureWindow{config.brewTemperatureWindow()}
, m_steamTemperatureWindow{config.steamTemperatureWindow()}
, m_readyHysteresisPeriod{config.heaterReadyHysteresisPeriod()}
, m_leds{std::move(leds)}
, m_temperatureSensor{core->getTemperatureSensor(libopenpresso_config_labels::TEMPERATURE_SENSOR_LABEL)}
{
}

StatusManager::~StatusManager() // NOLINT (bugprone-exception-escape)
{
  unregisterTemperatureCallback();
}

HeatingStatus StatusManager::getCurrentStatus() const noexcept
{
  return m_heatingStatus.load(std::memory_order_relaxed);
}

void StatusManager::disable()
{
  unregisterTemperatureCallback();

  m_leds->indicatePowerOff();

  publishStatus(HeatingStatus::HeatingStatus_DISABLED);
}

void StatusManager::unregisterTemperatureCallback()
{
  if (m_temperatureCallback.has_value()) {
    m_temperatureSensor->unregisterCallback(std::exchange(m_temperatureCallback, std::nullopt).value());
  }
}

void StatusManager::publishStatus(HeatingStatus status)
{
  if (status != m_heatingStatusCached) {
    m_heatingStatusCached = status;
    m_heatingStatus.store(status, std::memory_order_relaxed);
    std::invoke(m_callback, status);
  }
}

template <bool steamMode>
void StatusManager::applyStatus(HeatingStatus status)
{
  if constexpr (steamMode) {
    if (status == HeatingStatus::HeatingStatus_Preheat) {
      m_leds->indicateSteamPreheat();
    }
    else if (status == HeatingStatus::HeatingStatus_Overheat) {
      m_leds->indicateSteamOverheat();
    }
    else {
      m_leds->indicateSteamReady();
    }
  }
  else {
    if (status == HeatingStatus::HeatingStatus_Preheat) {
      m_leds->indicateBrewPreheat();
    }
    else if (status == HeatingStatus::HeatingStatus_Overheat) {
      m_leds->indicateBrewOverheat();
    }
    else {
      m_leds->indicateBrewReady();
    }
  }

  publishStatus(status);
}

HeatingStatus StatusManager::makeStatus(libopenpresso::millidegrees_t temperature,
                                        libopenpresso::millidegrees_t targetTemp,
                                        libopenpresso::millidegrees_t temperatureWindow)
{
  if (temperature + temperatureWindow < targetTemp) {
    return HeatingStatus::HeatingStatus_Preheat;
  }
  if (temperature > targetTemp + temperatureWindow) {
    return HeatingStatus::HeatingStatus_Overheat;
  }
  return HeatingStatus::HeatingStatus_Ready;
}

template <bool steamMode>
libopenpresso::interfaces::TemperatureSensor::callback_t StatusManager::makeFixedRateCallback(
  libopenpresso::millidegrees_t targetTemp,
  libopenpresso::millidegrees_t temperatureWindow,
  libopenpresso::time_delta_t updateRate)
{
  using FloatSeconds = std::chrono::duration<float>;
  auto callsPerPeriod = std::chrono::duration_cast<FloatSeconds>(m_readyHysteresisPeriod) / updateRate;
  const auto requiredInvocations = std::max<size_t>(1, std::ceil(callsPerPeriod));

  return [this, targetTemp, temperatureWindow, requiredInvocations, inRangeInvocations = std::size_t{0}](
           libopenpresso::millidegrees_t temperature) mutable {
    const auto status = makeStatus(temperature, targetTemp, temperatureWindow);
    if (status != HeatingStatus::HeatingStatus_Ready) {
      inRangeInvocations = 0;
      applyStatus<steamMode>(status);
      return;
    }

    if (m_heatingStatusCached == HeatingStatus::HeatingStatus_Ready ||
        (++inRangeInvocations) < requiredInvocations) {
      return;
    }

    applyStatus<steamMode>(HeatingStatus::HeatingStatus_Ready);
  };
}

template <bool steamMode>
libopenpresso::interfaces::TemperatureSensor::callback_t StatusManager::makeVariableRateCallback(
  libopenpresso::millidegrees_t targetTemp, libopenpresso::millidegrees_t temperatureWindow)
{
  using time_point_opt = std::optional<std::chrono::steady_clock::time_point>;
  return [this, targetTemp, temperatureWindow, inRangeSince = time_point_opt{}](
           libopenpresso::millidegrees_t temperature) mutable {
    const auto status = makeStatus(temperature, targetTemp, temperatureWindow);
    if (status != HeatingStatus::HeatingStatus_Ready) {
      inRangeSince.reset();
      applyStatus<steamMode>(status);
      return;
    }

    if (m_heatingStatusCached == HeatingStatus::HeatingStatus_Ready) {
      return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!inRangeSince.has_value()) {
      inRangeSince = now;
    }

    if (now - inRangeSince.value() >= m_readyHysteresisPeriod) {
      inRangeSince.reset();
      applyStatus<steamMode>(HeatingStatus::HeatingStatus_Ready);
    }
  };
}

template <bool steamMode>
libopenpresso::interfaces::TemperatureSensor::callback_t StatusManager::makeTemperatureCallback(
  libopenpresso::millidegrees_t targetTemp,
  libopenpresso::millidegrees_t temperatureWindow,
  std::optional<libopenpresso::time_delta_t> updateRate)
{
  if (updateRate.has_value()) {
    return makeFixedRateCallback<steamMode>(targetTemp, temperatureWindow, updateRate.value());
  }
  return makeVariableRateCallback<steamMode>(targetTemp, temperatureWindow);
}

void StatusManager::indicateBrewState(libopenpresso::millidegrees_t targetTemp)
{
  unregisterTemperatureCallback();
  applyStatus<false>(makeStatus(m_temperatureSensor->getTemperature(), targetTemp, m_brewTemperatureWindow));

  auto callback = makeTemperatureCallback<false>(targetTemp,
                                                 m_brewTemperatureWindow,
                                                 m_temperatureSensor->fixedUpdateRate());
  m_temperatureCallback = m_temperatureSensor->registerCallback(callback);
}

void StatusManager::indicateSteamState(libopenpresso::millidegrees_t targetTemp)
{
  unregisterTemperatureCallback();
  applyStatus<true>(makeStatus(m_temperatureSensor->getTemperature(), targetTemp, m_steamTemperatureWindow));

  auto callback = makeTemperatureCallback<true>(targetTemp,
                                                m_steamTemperatureWindow,
                                                m_temperatureSensor->fixedUpdateRate());
  m_temperatureCallback = m_temperatureSensor->registerCallback(callback);
}

void StatusManager::setCallback(std::function<void(HeatingStatus)> callback)
{
  m_callback = std::move(callback);
}

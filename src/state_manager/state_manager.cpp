#include "state_manager.hpp"

#include "config/libopenpresso_config_labels.hpp"
#include "leds/leds_handler_interface.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

#include <libopenpresso/interfaces/brew_profiler.hpp>      // IWYU pragma: keep
#include <libopenpresso/interfaces/controller_base.hpp>    // IWYU pragma: keep
#include <libopenpresso/interfaces/libopenpresso_core.hpp> // IWYU pragma: keep
#include <libopenpresso/interfaces/logical_input.hpp>      // IWYU pragma: keep
#include <libopenpresso/types.hpp>
#include <spdlog/spdlog.h>

using namespace openpressod;

StateManager::StateManager(const core_ptr_t& core, std::unique_ptr<LedsHandlerInterface>&& leds)
: m_steam{core->getLogicalInput(libopenpresso_config_labels::STEAM_BUTTON_LABEL)->getState()}
, m_brewProfiler{core->getBrewProfiler(libopenpresso_config_labels::BREW_PROFILER_LABEL)}
, m_brewTemperatureController{core->getTemperatureController(
    libopenpresso_config_labels::BREW_TEMPERATURE_CONTROLLER_LABEL)}
, m_steamController{core->getTemperatureController(libopenpresso_config_labels::STEAM_CONTROLLER_LABEL)}
, m_weightSensor{core->getWeightSensor(libopenpresso_config_labels::WEIGHT_SENSOR_LABEL)}
, m_leds{std::move(leds)}
{
  spdlog::debug("State manager created");
}

bool StateManager::getPowerState() const noexcept
{
  return m_power;
}

void StateManager::setPowerState(bool state)
{
  if (m_power == state) {
    spdlog::warn("Power is already {}", m_steam ? "ON" : "OFF");
    return;
  }

  spdlog::debug("Power state change from {} to: {}", m_power ? "ON" : "OFF", state ? "ON" : "OFF");

  if (m_power) {
    m_brewProfiler->deactivate();
    m_steamController->deactivate();
    m_brewTemperatureController->deactivate();
    m_leds->indicatePowerOff();
  }
  else if (m_steam) {
    m_steamController->activate();
    m_leds->indicateSteamState(m_steamController->getTargetTemperature());
  }
  else {
    m_brewTemperatureController->activate();
    m_leds->indicateBrewState(m_brewTemperatureController->getTargetTemperature());
  }

  m_power = state;
}

bool StateManager::getBrewState() const noexcept
{
  return m_brewProfiler->isActive();
}

void StateManager::startBrew()
{
  if (m_brewProfiler->isActive()) {
    spdlog::warn("Brew process is already running");
    return;
  }

  spdlog::debug("Starting brew process");

  if (!m_power) {
    throw std::runtime_error{"Cannot start brew while power is off"};
  }

  if (m_steam) {
    throw std::runtime_error{"Cannot start brew in steam mode"};
  }

  m_brewProfiler->activate();

  spdlog::debug("Brew process started");
}

void StateManager::stopBrew()
{
  if (!m_brewProfiler->isActive()) {
    spdlog::warn("Brew process is already stopped");
    return;
  }

  spdlog::debug("Stopping brew process");

  m_brewProfiler->deactivate();

  spdlog::debug("Brew process stopped");
}

void StateManager::setBrewTemperature(libopenpresso::millidegrees_t temperature)
{
  m_brewTemperatureController->setTargetTemperature(temperature);
  if (!m_steam && m_power) {
    m_leds->indicateBrewState(temperature);
  }
  spdlog::debug("Brew temperature changed, new value: {}", temperature);
}

void StateManager::setSteamTemperature(libopenpresso::millidegrees_t temperature)
{
  m_steamController->setTargetTemperature(temperature);
  if (m_steam && m_power) {
    m_leds->indicateSteamState(temperature);
  }
  spdlog::debug("Steam temperature changed, new value: {}", temperature);
}

bool StateManager::getSteamModeState() const noexcept
{
  spdlog::debug("Steam mode state requsted, state: {}", m_steam ? "ON" : "OFF");
  return m_steam;
}

void StateManager::setSteamModeState(bool state)
{
  if (m_steam == state) {
    spdlog::warn("Steam mode is already {}", m_steam ? "ON" : "OFF");
    return;
  }

  spdlog::debug("Steam mode state change from {} to: {}", m_steam ? "ON" : "OFF", state ? "ON" : "OFF");

  if (m_brewProfiler->isActive()) {
    throw std::runtime_error{"Cannot switch to steam mode while brewing"};
  }

  if (m_power) {
    if (m_steam) {
      m_steamController->deactivate();
      m_brewTemperatureController->activate();
      m_leds->indicateBrewState(m_brewTemperatureController->getTargetTemperature());
    }
    else {
      m_brewTemperatureController->deactivate();
      m_steamController->activate();
      m_leds->indicateSteamState(m_steamController->getTargetTemperature());
    }
  }

  m_steam = state;
}

void StateManager::resetScales()
{
  if (m_brewProfiler->isActive()) {
    throw std::runtime_error{"Cannot reset scale while brewing"};
  }
  m_weightSensor->tare();
}

void StateManager::unregisterBrewProfileCallback(libopenpresso::callback_descriptor_t descr)
{
  m_brewProfiler->unregisterStepChangeCallback(descr);
}

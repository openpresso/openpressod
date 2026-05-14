#include "buttons_callback_registrator.hpp"

#include "buttons_event_sink.hpp"
#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"

#include <memory>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/interfaces/logical_input.hpp>
#include <libopenpresso/types.hpp>
#include <spdlog/spdlog.h>

using namespace openpressod;

ButtonsCallbackRegistrator::ButtonsCallbackRegistrator(const std::shared_ptr<ButtonsEventSink>& sink,
                                                       const libopenpresso::CorePtr& core,
                                                       const OpenpressodConfig& config)
: m_sink{sink}
, m_powerButton{core->getLogicalInput(libopenpresso_config_labels::POWER_BUTTON_LABEL)}
, m_brewButton{core->getLogicalInput(libopenpresso_config_labels::BREW_BUTTON_LABEL)}
, m_steamButton{core->getLogicalInput(libopenpresso_config_labels::STEAM_BUTTON_LABEL)}
, m_powerButtonCallback{registerPowerButtonCallback(config.powerButtonBistable())}
, m_brewButtonCallback{registerBrewButtonCallback(config.brewButtonBistable())}
, m_steamButtonCallback{registerSteamButtonCallback(config.steamButtonBistable())}
{
  spdlog::debug("Buttons handller created");
}

ButtonsCallbackRegistrator::~ButtonsCallbackRegistrator()
{
  m_powerButton->unregisterCallback(m_powerButtonCallback);
  m_brewButton->unregisterCallback(m_brewButtonCallback);
  m_steamButton->unregisterCallback(m_steamButtonCallback);
}

libopenpresso::callback_descriptor_t ButtonsCallbackRegistrator::registerPowerButtonCallback(bool bistable)
{
  if (bistable) {
    return m_powerButton->registerCallback([this](bool state) { m_sink->setPowerState(state); });
  }

  return m_powerButton->registerCallback([this](bool state) {
    if (state) {
      m_sink->togglePowerState();
    }
  });
}

libopenpresso::callback_descriptor_t ButtonsCallbackRegistrator::registerBrewButtonCallback(bool bistable)
{
  if (bistable) {
    return m_brewButton->registerCallback([this](bool state) {
      if (state) {
        m_sink->startBrew();
      }
      else {
        m_sink->stopBrew();
      }
    });
  }
  return m_brewButton->registerCallback([this](bool state) {
    if (state) {
      m_sink->toggleBrew();
    }
  });
}

libopenpresso::callback_descriptor_t ButtonsCallbackRegistrator::registerSteamButtonCallback(bool bistable)
{
  if (bistable) {
    return m_steamButton->registerCallback([this](bool state) { m_sink->setSteamModeState(state); });
  }
  return m_steamButton->registerCallback([this](bool state) {
    if (state) {
      m_sink->toggleSteamModeState();
    }
  });
}

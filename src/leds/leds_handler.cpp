#include "leds_handler.hpp"

#include "config/libopenpresso_config_labels.hpp"

#include <cmath>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/types.hpp>
#include <spdlog/spdlog.h>

using namespace openpressod;

LedsHandler::LedsHandler(const libopenpresso::CorePtr& core)
: m_power(core->getLogicalOutput(libopenpresso_config_labels::POWER_LED_LABEL))
, m_brew(core->getLogicalOutput(libopenpresso_config_labels::BREW_LED_LABEL))
, m_steam(core->getLogicalOutput(libopenpresso_config_labels::STEAM_LED_LABEL))
{
  m_power->activate();
  m_brew->activate();
  m_steam->activate();

  spdlog::debug("Leds manager created");
}

LedsHandler::~LedsHandler() // NOLINT (bugprone-exception-escape)
{
  m_power->deactivate();
  m_brew->deactivate();
  m_steam->deactivate();
}

void LedsHandler::indicatePowerOff()
{
  m_power->setState(false);
  m_brew->setState(false);
  m_steam->setState(false);
}

void LedsHandler::indicateBrewPreheat()
{
  m_power->setState(true);
  m_brew->setState(false);
  m_steam->setState(false);
}

void LedsHandler::indicateBrewReady()
{
  m_power->setState(true);
  m_brew->setState(true);
  m_steam->setState(false);
}

void LedsHandler::indicateBrewOverheat()
{
  m_power->setState(true);
  m_brew->setState(false);
  m_steam->setState(false);
}

void LedsHandler::indicateSteamPreheat()
{
  m_power->setState(true);
  m_brew->setState(false);
  m_steam->setState(false);
}

void LedsHandler::indicateSteamReady()
{
  m_power->setState(true);
  m_brew->setState(false);
  m_steam->setState(true);
}

void LedsHandler::indicateSteamOverheat()
{
  m_power->setState(true);
  m_brew->setState(false);
  m_steam->setState(false);
}

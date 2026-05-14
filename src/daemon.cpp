#include "daemon.hpp"

#include "async_events_dispatcher/async_events_dispatcher.hpp"
#include "buttons/buttons_callback_registrator.hpp"
#include "config/libopenpresso_config_maker.hpp"
#include "config/openpressod_config.hpp"
#include "leds/leds_handler.hpp"
#include "service/service_manager.hpp"
#include "state_manager/brew_profile_manager.hpp"
#include "state_manager/state_manager.hpp"
#include "state_manager/user_settings_manager.hpp"

#include <exception>
#include <memory>
#include <utility>

#include <libopenpresso/libopenpresso.hpp>
#include <spdlog/spdlog.h>

using namespace openpressod;

Daemon::Daemon()
{
  auto config = OpenpressodConfig::fromFile();
  setupLogger(config);

  auto deviceConfig = LibopenpressoConfigMaker(config, spdlog::default_logger()).make();
  spdlog::debug("Device config created");

  auto core = libopenpresso::getCore(deviceConfig);
  auto leds = std::make_unique<LedsHandler>(core, config);
  auto stateManager = std::make_unique<StateManager>(core, std::move(leds));
  auto profileManager = setupBrewProfileManager(config);
  auto settingsManager = setupUserSettingsManager(config);

  profileManager->applyBrewProfile(*stateManager);
  settingsManager->applySettings(*stateManager);

  auto dispatcher = std::make_shared<AsyncEventDispatcher>(std::move(stateManager),
                                                           std::move(profileManager),
                                                           std::move(settingsManager));

  m_buttonsHandler = std::make_unique<ButtonsCallbackRegistrator>(dispatcher, core, config);
  m_service = std::make_unique<ServiceManager>(dispatcher, core, config);
}

std::unique_ptr<BrewProfileManager> Daemon::setupBrewProfileManager(const OpenpressodConfig& config)
{
  auto profileManager = std::make_unique<BrewProfileManager>(config);
  try {
    profileManager->loadSavedProfile();
  }
  catch (const std::exception& e) {
    spdlog::warn(e.what());
    profileManager->loadDefaultProfile();
  }
  return std::move(profileManager);
}

std::unique_ptr<UserSettingsManager> Daemon::setupUserSettingsManager(const OpenpressodConfig& config)
{
  auto settingsManager = std::make_unique<UserSettingsManager>(config);
  try {
    settingsManager->loadSavedSettings();
  }
  catch (const std::exception& e) {
    spdlog::warn(e.what());
    settingsManager->loadDefaultSettings();
  }
  return std::move(settingsManager);
}

void Daemon::setupLogger(const OpenpressodConfig& config)
{
  spdlog::set_level(config.logLevel());
  spdlog::set_pattern(config.logMessagePattern());
  spdlog::debug("Logger settings updated");
}

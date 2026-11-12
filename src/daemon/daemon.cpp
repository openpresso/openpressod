#include "daemon.hpp"

#include "async_events_dispatcher/async_events_dispatcher.hpp"
#include "brew_history/brew_history_manager.hpp"
#include "buttons/buttons_callback_registrator.hpp"
#include "config/libopenpresso_config_maker.hpp"
#include "config/openpressod_config.hpp"
#include "database/brew_history/async_repository.hpp"
#include "database/brew_history/repository.hpp"
#include "database/brew_profiles/async_repository.hpp"
#include "leds/leds_handler.hpp"
#include "service/brew_history_stream_reactor.hpp"
#include "service/service_manager.hpp"
#include "state_manager/brew_profile_manager.hpp"
#include "state_manager/state_manager.hpp"
#include "state_manager/status_manager.hpp"
#include "state_manager/user_settings_manager.hpp"

#include <exception>
#include <memory>
#include <string>
#include <utility>

#include <libopenpresso/libopenpresso.hpp>
#include <spdlog/spdlog.h>

using namespace openpressod;

Daemon::Daemon(const std::string& configPath)
{
  auto config = OpenpressodConfig::fromFile(configPath);
  setupLogger(config);

  auto deviceConfig = LibopenpressoConfigMaker(config, spdlog::default_logger()).make();
  spdlog::debug("Device config created");

  auto core = libopenpresso::getCore(deviceConfig);
  auto leds = std::make_unique<LedsHandler>(core);
  auto statusManager = std::make_unique<StatusManager>(core, config, std::move(leds));
  auto stateManager = std::make_unique<StateManager>(core, std::move(statusManager));
  auto profileManager = setupBrewProfileManager(config);
  auto settingsManager = setupUserSettingsManager(config);
  auto brewTimer = stateManager->brewTimer();
  auto profilesRepository =
    std::make_shared<db::brew_profiles::AsyncBrewProfilesRepository>(config.profilesDatabasePath());

  auto historyRepository = setupHistoryRepository(config);

  profileManager->applyBrewProfile(*stateManager);
  settingsManager->applySettings(*stateManager);

  auto brewHistoryManager = std::make_shared<BrewHistoryManager>(core, historyRepository, config);

  auto dispatcher = std::make_shared<AsyncEventDispatcher>(std::move(stateManager),
                                                           std::move(profileManager),
                                                           std::move(settingsManager),
                                                           brewHistoryManager,
                                                           historyRepository);

  m_buttonsHandler = std::make_unique<ButtonsCallbackRegistrator>(dispatcher, core, config);
  m_service = std::make_unique<ServiceManager>(
    dispatcher, brewHistoryManager, core, std::move(brewTimer), profilesRepository, historyRepository, config);
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
  return profileManager;
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
  return settingsManager;
}

void Daemon::setupLogger(const OpenpressodConfig& config)
{
  spdlog::set_level(config.logLevel());
  spdlog::set_pattern(config.logMessagePattern());
  spdlog::debug("Logger settings updated");
}

std::shared_ptr<db::brew_history::AsyncBrewHistoryRepository> Daemon::setupHistoryRepository(
  const OpenpressodConfig& config)
{
  db::brew_history::BrewHistoryRepository historyRepository{config.historyDatabasePath()};
  historyRepository.discardIncomplete();
  return std::make_shared<db::brew_history::AsyncBrewHistoryRepository>(std::move(historyRepository));
}

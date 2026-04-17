#include "async_events_dispatcher/async_events_dispatcher.hpp"
#include "buttons/buttons_callback_registrator.hpp"
#include "config/libopenpresso_config_maker.hpp"
#include "config/openpressod_config.hpp"
#include "leds/leds_handler.hpp"
#include "service/service_manager.hpp"
#include "signals_handler/signals_handler.hpp"
#include "state_manager/state_manager.hpp"

#include <atomic>
#include <exception>
#include <memory>
#include <utility>

#include <libopenpresso/libopenpresso.hpp>
#include <spdlog/spdlog.h>

using namespace openpressod;

namespace
{

void runDaemon(const std::atomic<bool>& exitFlag)
{
  auto config = OpenpressodConfig::fromFile();
  auto deviceConfig = LibopenpressoConfigMaker(config, spdlog::default_logger()).make();
  auto core = libopenpresso::getCore(deviceConfig);
  auto leds = std::make_unique<LedsHandler>(core, config);
  auto stateManager = std::make_unique<StateManager>(core, config, std::move(leds));
  auto dispatcher = std::make_shared<AsyncEventDispatcher>(std::move(stateManager));

  ButtonsCallbackRegistrator buttonsCallbacks{dispatcher, core, config};
  ServiceManager service{dispatcher, core, config};

  exitFlag.wait(false, std::memory_order_relaxed);
}

} // namespace

int main()
{
  SignalsHandler::init();

  try {
    runDaemon(SignalsHandler::exitFlag());
  }
  catch (const std::exception& e) {
    spdlog::critical("daemon will be stopped due to unhandled exception: {}", e.what());
    throw;
  }

  spdlog::info("daemon finished");

  return 0;
}

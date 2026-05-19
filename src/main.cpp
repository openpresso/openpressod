#include "daemon/daemon.hpp"
#include "signals_handler/signals_handler.hpp"

#include <atomic>
#include <exception>

#include <spdlog/spdlog.h>

int main()
{
  spdlog::info("Starting openpresso daemon");
  openpressod::SignalsHandler::init();

  try {
    openpressod::Daemon daemon;
    spdlog::info("Waiting for exit signal...");
    openpressod::SignalsHandler::exitFlag().wait(false, std::memory_order_relaxed);
    spdlog::info("Exit signal received");
  }
  catch (const std::exception& e) {
    spdlog::critical("daemon will be stopped due to unhandled exception: {}", e.what());
    throw;
  }

  spdlog::info("Daemon finished");

  return 0;
}

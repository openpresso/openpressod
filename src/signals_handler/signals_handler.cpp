#include "signals_handler.hpp"

#include <atomic>
#include <csignal>
#include <stdexcept>

using namespace openpressod;

std::atomic<bool> SignalsHandler::s_exit = false;

void SignalsHandler::exitSignalsHandler([[maybe_unused]] int signal)
{
  s_exit.store(true, std::memory_order_relaxed);
  s_exit.notify_all();
}

void SignalsHandler::registerExitHandler(int signal)
{
  if (std::signal(signal, exitSignalsHandler) == SIG_ERR) {
    throw std::runtime_error{"Failed to register signal handler"};
  }
}

void SignalsHandler::init()
{
  registerExitHandler(SIGINT);
  registerExitHandler(SIGTERM);
  registerExitHandler(SIGHUP); // NOLINT(misc-include-cleaner)
}

const std::atomic<bool>& openpressod::SignalsHandler::exitFlag() noexcept
{
  return s_exit;
}

#include "daemon/daemon.hpp"
#include "signals_handler/signals_handler.hpp"

#include <atomic>
#include <exception>
#include <string>
#include <sysexits.h>

#include <CLI/CLI.hpp>
#include <libopenpresso/exception.hpp>
#include <spdlog/spdlog.h>

int main(int argc, char** argv)
{
  std::string configPath;
  try {
    CLI::App app{"Openpresso Daemon"};
    app.set_version_flag("-v,--version", OPENPRESSOD_VERSION);
    app.add_option("-c,--conf", configPath, "Path to daemon config")
      ->envname("OPENPRESSOD_CONFIG_PATH")
      ->default_val(OPENPRESSOD_CONFIG_PATH);
    CLI11_PARSE(app, argc, argv);
  }
  catch (const CLI::Error& e) {
    spdlog::critical("CLI args parse failed, daemon won't be started: {}", e.what());
    return EX_SOFTWARE;
  }

  spdlog::info("Starting openpresso daemon");
  openpressod::SignalsHandler::init();

  try {
    openpressod::Daemon daemon{configPath};
    spdlog::info("Waiting for exit signal...");
    openpressod::SignalsHandler::exitFlag().wait(false, std::memory_order_relaxed);
    spdlog::info("Exit signal received");
  }
  catch (const libopenpresso::Exception& e) {
    spdlog::critical("daemon will be stopped due to unhandled libopenpresso exception: \"{}\", "
                     "from {}:{} at line {}",
                     e.what(),
                     e.throwLocation().file_name(),
                     e.throwLocation().function_name(),
                     e.throwLocation().line());
    return EX_SOFTWARE;
  }
  catch (const std::exception& e) {
    spdlog::critical("daemon will be stopped due to unhandled exception: {}", e.what());
    return EX_SOFTWARE;
  }

  spdlog::info("Daemon finished");

  return EX_OK;
}

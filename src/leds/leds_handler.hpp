#ifndef LEDS_HANDLER_HPP
#define LEDS_HANDLER_HPP

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/interfaces/logical_output.hpp>

namespace openpressod
{

class LedsHandler {
public:
  LedsHandler(const libopenpresso::CorePtr& core);
  LedsHandler(const LedsHandler&) = delete;
  LedsHandler(LedsHandler&&) = delete;
  auto operator=(const LedsHandler&) = delete;
  auto operator=(LedsHandler&&) = delete;
  ~LedsHandler();

  void indicatePowerOff();

  void indicateBrewPreheat();
  void indicateBrewReady();
  void indicateBrewOverheat();

  void indicateSteamPreheat();
  void indicateSteamReady();
  void indicateSteamOverheat();

private:
  libopenpresso::LogicalOutputPtr m_power;
  libopenpresso::LogicalOutputPtr m_brew;
  libopenpresso::LogicalOutputPtr m_steam;
};

} // namespace openpressod

#endif // LEDS_HANDLER_HPP
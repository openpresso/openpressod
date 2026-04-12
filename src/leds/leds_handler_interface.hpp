#ifndef LEDS_HANDLER_INTERFACE_HPP
#define LEDS_HANDLER_INTERFACE_HPP

#include <libopenpresso/types.hpp>

namespace openpressod
{

class LedsHandlerInterface {
public:
  virtual ~LedsHandlerInterface() = default;

  virtual void indicatePowerOff() = 0;
  virtual void indicateBrewState(libopenpresso::millidegrees_t targetTemp) = 0;
  virtual void indicateSteamState() = 0;
};

} // namespace openpressod

#endif // LEDS_HANDLER_INTERFACE_HPP
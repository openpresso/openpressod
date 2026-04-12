#ifndef LEDS_HANDLER_MOCK_HPP
#define LEDS_HANDLER_MOCK_HPP

#include <gmock/gmock.h>
#include "leds/leds_handler_interface.hpp"

namespace openpressod
{

class LedsHandlerMock : public LedsHandlerInterface
{
public:
    MOCK_METHOD(void, indicatePowerOff, (), (override));
    MOCK_METHOD(void, indicateBrewState, (libopenpresso::millidegrees_t targetTemp), (override));
    MOCK_METHOD(void, indicateSteamState, (), (override));
};

} // namespace openpressod

#endif // LEDS_HANDLER_MOCK_HPP

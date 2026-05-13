#ifndef BUTTONS_EVENT_SINK_MOCK_HPP
#define BUTTONS_EVENT_SINK_MOCK_HPP

#include "buttons/buttons_event_sink.hpp"

#include <gmock/gmock.h>

namespace openpressod
{

class ButtonsEventSinkMock : public ButtonsEventSink {
public:
  MOCK_METHOD(void, togglePowerState, (), (override));
  MOCK_METHOD(void, setPowerState, (bool state), (override));
  MOCK_METHOD(void, toggleBrew, (), (override));
  MOCK_METHOD(void, startBrew, (), (override));
  MOCK_METHOD(void, stopBrew, (), (override));
  MOCK_METHOD(void, toggleSteamModeState, (), (override));
  MOCK_METHOD(void, setSteamModeState, (bool state), (override));
};

} // namespace openpressod

#endif // BUTTONS_EVENT_SINK_MOCK_HPP

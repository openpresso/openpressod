#ifndef BUTTONS_EVENT_SINK_HPP
#define BUTTONS_EVENT_SINK_HPP

namespace openpressod
{

class ButtonsEventSink {
public:
  virtual void togglePowerState() = 0;
  virtual void setPowerState(bool state) = 0;

  virtual void toggleBrew() = 0;
  virtual void startBrew() = 0;
  virtual void stopBrew() = 0;

  virtual void toggleSteamModeState() = 0;
  virtual void setSteamModeState(bool state) = 0;

  virtual ~ButtonsEventSink() = default;
};

} // namespace openpressod

#endif // BUTTONS_EVENT_SINK_HPP
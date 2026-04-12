#ifndef ASYNC_EVENTS_DISPATCHER_HPP
#define ASYNC_EVENTS_DISPATCHER_HPP

#include "buttons/buttons_event_sink.hpp"
#include "procedures_serial_executor.hpp"
#include "state_manager/state_manager.hpp"

#include <memory>

namespace openpressod
{

class AsyncEventDispatcher : public ButtonsEventSink {
public:
  explicit AsyncEventDispatcher(std::unique_ptr<StateManager>&& stateManager);

  void togglePowerState() override;
  void setPowerState(bool state) override;

  void toggleBrew() override;
  void startBrew() override;
  void stopBrew() override;

  void toggleSteamModeState() override;
  void setSteamModeState(bool state) override;

private:
  std::unique_ptr<StateManager> m_stateManager;
  ProceduresSerialExecutor m_executor;
};

} // namespace openpressod

#endif // ASYNC_EVENTS_DISPATCHER_HPP
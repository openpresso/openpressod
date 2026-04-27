#ifndef ASYNC_EVENTS_DISPATCHER_HPP
#define ASYNC_EVENTS_DISPATCHER_HPP

#include "buttons/buttons_event_sink.hpp"
#include "procedures_serial_executor.hpp"
#include "state_manager/state_manager.hpp"

#include <memory>

#include <libopenpresso/interfaces/brew_profiler.hpp>
#include <libopenpresso/types.hpp>

namespace openpressod
{

class BrewProfile;
class EventsStreamReactor;

class AsyncEventDispatcher : public ButtonsEventSink {
  using step_index_t = libopenpresso::interfaces::BrewProfiler::step_index_t;
  using stopped_flag_t = libopenpresso::interfaces::BrewProfiler::stopped_flag_t;

public:
  AsyncEventDispatcher(std::unique_ptr<StateManager>&& stateManager);
  AsyncEventDispatcher(const AsyncEventDispatcher&) = delete;
  AsyncEventDispatcher(AsyncEventDispatcher&&) = delete;
  auto operator=(const AsyncEventDispatcher&) = delete;
  auto operator=(AsyncEventDispatcher&&) = delete;
  ~AsyncEventDispatcher();

  void togglePowerState() override;
  void setPowerState(bool state) override;

  void toggleBrew() override;
  void startBrew() override;
  void stopBrew() override;

  void toggleSteamModeState() override;
  void setSteamModeState(bool state) override;

  template <typename Cb>
  void resetWeight(Cb&& callback);

  template <typename Cb>
  void getBrewProfileInfo(Cb&& callback);

  template <typename Cb>
  void setBrewProfile(const BrewProfile* profile, Cb&& callback);

  template <typename Cb>
  void getPowerState(Cb&& callback);

  template <typename Cb>
  void setPowerState(bool state, Cb&& callback);

  template <typename Cb>
  void getBrewState(Cb&& callback);

  template <typename Cb>
  void startBrew(Cb&& callback);

  template <typename Cb>
  void stopBrew(Cb&& callback);

  template <typename Cb>
  void getSteamModeState(Cb&& callback);

  template <typename Cb>
  void setSteamModeState(bool state, Cb&& callback);

  void addEventsStreamReactor(std::unique_ptr<EventsStreamReactor> reactor);
  void releaseEventsStreamReactor(const EventsStreamReactor* reactor);

  void brewCallback(std::variant<step_index_t, stopped_flag_t> step);

private:
  std::unique_ptr<StateManager> m_stateManager;
  ProceduresSerialExecutor m_executor;
  libopenpresso::callback_descriptor_t m_brewCallback;
};

template <typename Cb>
inline void AsyncEventDispatcher::resetWeight(Cb&& callback)
{
  m_executor.executeWithCallback(&StateManager::resetScales,
                                 std::forward<Cb>(callback),
                                 m_stateManager.get());
}

template <typename Cb>
inline void AsyncEventDispatcher::getBrewProfileInfo(Cb&& callback)
{
  m_executor.executeWithCallback(
    [stateManager = m_stateManager.get()] {
      return std::make_pair(stateManager->brewProfileName(), stateManager->brewProfileHash());
    },
    std::forward<Cb>(callback));
}

template <typename Cb>
inline void AsyncEventDispatcher::setBrewProfile(const BrewProfile* profile, Cb&& callback)
{
  m_executor.executeWithCallback(
    [stateManager = m_stateManager.get(), profile] {
      stateManager->setBrewProfile(profile);
      return stateManager->brewProfileHash();
    },
    std::forward<Cb>(callback));
}

template <typename Cb>
inline void AsyncEventDispatcher::getPowerState(Cb&& callback)
{
  m_executor.executeWithCallback(&StateManager::getPowerState,
                                 std::forward<Cb>(callback),
                                 m_stateManager.get());
}

template <typename Cb>
inline void AsyncEventDispatcher::setPowerState(bool state, Cb&& callback)
{
  m_executor.executeWithCallback(
    &StateManager::setPowerState, std::forward<Cb>(callback), m_stateManager.get(), state);
}

template <typename Cb>
inline void AsyncEventDispatcher::getBrewState(Cb&& callback)
{
  m_executor.executeWithCallback(&StateManager::getBrewState,
                                 std::forward<Cb>(callback),
                                 m_stateManager.get());
}

template <typename Cb>
inline void AsyncEventDispatcher::startBrew(Cb&& callback)
{
  m_executor.executeWithCallback(&StateManager::startBrew,
                                 std::forward<Cb>(callback),
                                 m_stateManager.get());
}

template <typename Cb>
inline void AsyncEventDispatcher::stopBrew(Cb&& callback)
{
  m_executor.executeWithCallback(&StateManager::stopBrew, std::forward<Cb>(callback), m_stateManager.get());
}

template <typename Cb>
inline void AsyncEventDispatcher::getSteamModeState(Cb&& callback)
{
  m_executor.executeWithCallback(&StateManager::getSteamModeState,
                                 std::forward<Cb>(callback),
                                 m_stateManager.get());
}

template <typename Cb>
inline void AsyncEventDispatcher::setSteamModeState(bool state, Cb&& callback)
{
  m_executor.executeWithCallback(
    &StateManager::setSteamModeState, std::forward<Cb>(callback), m_stateManager.get(), state);
}

} // namespace openpressod

#endif // ASYNC_EVENTS_DISPATCHER_HPP
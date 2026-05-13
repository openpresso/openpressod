#ifndef ASYNC_EVENTS_DISPATCHER_HPP
#define ASYNC_EVENTS_DISPATCHER_HPP

#include "buttons/buttons_event_sink.hpp"
#include "procedures_serial_executor.hpp"
#include "service/events_stream_reactor.hpp"
#include "state_manager/brew_profile_manager.hpp"
#include "state_manager/state_manager.hpp"
#include "state_manager/user_settings_manager.hpp"

#include <list>
#include <memory>
#include <utility>
#include <variant>

#include <libopenpresso/types.hpp>
#include <openpresso_proto/openpresso.pb.h>

namespace openpressod
{

class BrewProfile;
class EventsStreamReactor;

class AsyncEventDispatcher : public ButtonsEventSink {
  using step_index_t = libopenpresso::interfaces::BrewProfiler::step_index_t;
  using stopped_flag_t = libopenpresso::interfaces::BrewProfiler::stopped_flag_t;

public:
  AsyncEventDispatcher(std::unique_ptr<StateManager> stateManager,
                       std::unique_ptr<BrewProfileManager> brewProfileManager,
                       std::unique_ptr<UserSettingsManager> userSettingsManager);
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
  void getBrewProfile(Cb&& callback);

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

  template <typename Cb>
  void getUserSettings(Cb&& callback);

  template <typename Cb>
  void setUserSettings(const UserSettings* settings, Cb&& callback);

  void addEventsStreamReactor(std::unique_ptr<EventsStreamReactor> reactor);
  void releaseEventsStreamReactor(const EventsStreamReactor* reactor);

  void brewCallback(std::variant<step_index_t, stopped_flag_t> step);

private:
  std::unique_ptr<StateManager> m_stateManager;
  std::unique_ptr<BrewProfileManager> m_brewProfileManager;
  std::unique_ptr<UserSettingsManager> m_userSettings;
  std::list<std::unique_ptr<EventsStreamReactor>> m_eventsSinks;
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
inline void AsyncEventDispatcher::getBrewProfile(Cb&& callback)
{
  m_executor.executeWithCallback(&BrewProfileManager::getProfile,
                                 std::forward<Cb>(callback),
                                 m_brewProfileManager.get());
}

template <typename Cb>
inline void AsyncEventDispatcher::setBrewProfile(const BrewProfile* profile, Cb&& callback)
{
  m_executor.executeWithCallback(
    [this, profile] {
      m_brewProfileManager->saveProfile(*profile);
      m_stateManager->applyProfile(profile);
      for (auto&& sink : m_eventsSinks) {
        sink->notifyChanged(*profile);
      }
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
    [this, state] {
      m_stateManager->setPowerState(state);
      PowerState stateChange;
      stateChange.set_value(state);
      for (auto&& sink : m_eventsSinks) {
        sink->notifyChanged(stateChange);
      }
    },
    std::forward<Cb>(callback));
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
  m_executor.executeWithCallback(
    [this] {
      m_stateManager->stopBrew();
      BrewProgress noProgress;
      for (auto&& sink : m_eventsSinks) {
        sink->notifyChanged(noProgress);
      }
    },
    std::forward<Cb>(callback));
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
    [this, state] {
      m_stateManager->setSteamModeState(state);
      SteamModeState stateChange;
      stateChange.set_isactive(state);
      for (auto&& sink : m_eventsSinks) {
        sink->notifyChanged(stateChange);
      }
    },
    std::forward<Cb>(callback));
}

template <typename Cb>
inline void AsyncEventDispatcher::getUserSettings(Cb&& callback)
{
  m_executor.executeWithCallback(&UserSettingsManager::getSettings,
                                 std::forward<Cb>(callback),
                                 m_userSettings.get());
}

template <typename Cb>
inline void AsyncEventDispatcher::setUserSettings(const UserSettings* settings, Cb&& callback)
{
  m_executor.executeWithCallback(
    [this, settings] {
      m_userSettings->saveSettings(*settings);
      m_stateManager->setSteamTemperature(settings->steamttemperature());
    },
    std::forward<Cb>(callback));
}

} // namespace openpressod

#endif // ASYNC_EVENTS_DISPATCHER_HPP
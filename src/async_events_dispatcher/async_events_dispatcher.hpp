#ifndef ASYNC_EVENTS_DISPATCHER_HPP
#define ASYNC_EVENTS_DISPATCHER_HPP

#include "brew_log/brew_log_manager.hpp"
#include "buttons/buttons_event_sink.hpp"
#include "service/events_stream_reactor.hpp"
#include "state_manager/brew_profile_manager.hpp"
#include "state_manager/state_manager.hpp"
#include "state_manager/user_settings_manager.hpp"
#include "utils/procedures_serial_executor.hpp"

#include <list>
#include <memory>
#include <stdexcept>
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
                       std::unique_ptr<UserSettingsManager> userSettingsManager,
                       std::shared_ptr<BrewLogManager> brewLogManager);
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

private:
  void doSetPowerState(bool state);
  void doSetSteamModeState(bool state);
  void doStartBrew();
  void doStopBrew();
  void doNotifyBrewStepChange(step_index_t step);
  void sendInitialState(const std::unique_ptr<EventsStreamReactor>& sink);
  void brewCallback(std::variant<step_index_t, stopped_flag_t> step);
  void notifyHeatingStatusChange(HeatingStatus status);

private:
  size_t m_brewProfileGeneration = 0;
  std::unique_ptr<StateManager> m_stateManager;
  std::unique_ptr<BrewProfileManager> m_brewProfileManager;
  std::unique_ptr<UserSettingsManager> m_userSettings;
  std::shared_ptr<BrewLogManager> m_brewLogManager;
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
  m_executor.executeWithCallback(
    [this] {
      BrewProfileInfo profileInfo;
      profileInfo.mutable_profile()->CopyFrom(m_brewProfileManager->getProfile());
      profileInfo.set_generation(m_brewProfileGeneration);
      return profileInfo;
    },
    std::forward<Cb>(callback));
}

template <typename Cb>
inline void AsyncEventDispatcher::setBrewProfile(const BrewProfile* profile, Cb&& callback)
{
  m_executor.executeWithCallback(
    [this, profile] {
      if (m_stateManager->getBrewState()) {
        throw std::runtime_error{"Cannot change profile while brewing"};
      }
      m_brewProfileManager->saveProfile(*profile);
      m_brewProfileManager->applyBrewProfile(*m_stateManager);

      BrewProfileInfo profileUpdate;
      profileUpdate.mutable_profile()->CopyFrom(*profile);
      profileUpdate.set_generation(++m_brewProfileGeneration);

      for (auto&& sink : m_eventsSinks) {
        sink->notifyChanged(profileUpdate);
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
  m_executor.executeWithCallback(&AsyncEventDispatcher::doSetPowerState, std::forward<Cb>(callback), this, state);
}

template <typename Cb>
inline void AsyncEventDispatcher::getBrewState(Cb&& callback)
{
  m_executor.executeWithCallback(
    [this] {
      BrewState state;
      state.set_isactive(m_stateManager->getBrewState());
      state.set_brewid(m_stateManager->brewGeneration());
      state.set_profilegeneration(m_brewProfileGeneration);
      return state;
    },
    std::forward<Cb>(callback));
}

template <typename Cb>
inline void AsyncEventDispatcher::startBrew(Cb&& callback)
{
  m_executor.executeWithCallback(&AsyncEventDispatcher::doStartBrew, std::forward<Cb>(callback), this);
}

template <typename Cb>
inline void AsyncEventDispatcher::stopBrew(Cb&& callback)
{
  m_executor.executeWithCallback(&AsyncEventDispatcher::doStopBrew, std::forward<Cb>(callback), this);
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
    &AsyncEventDispatcher::doSetSteamModeState, std::forward<Cb>(callback), this, state);
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
      m_userSettings->applySettings(*m_stateManager);
      for (auto&& sink : m_eventsSinks) {
        sink->notifyChanged(*settings);
      }
    },
    std::forward<Cb>(callback));
}

} // namespace openpressod

#endif // ASYNC_EVENTS_DISPATCHER_HPP

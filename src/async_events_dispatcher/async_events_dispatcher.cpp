#include "async_events_dispatcher.hpp"

#include "brew_history/brew_history_manager.hpp"
#include "database/brew_history/async_repository.hpp" // IWYU pragma: keep
#include "service/events_stream_reactor.hpp"
#include "state_manager/brew_profile_manager.hpp"
#include "state_manager/state_manager.hpp"
#include "state_manager/user_settings_manager.hpp"
#include "utils/lambda_overload.hpp"

#include <memory>
#include <utility>
#include <variant>

#include <openpresso_proto/brew_profile.pb.h>
#include <openpresso_proto/machine_state.pb.h>
#include <spdlog/spdlog.h>

using namespace openpressod;

AsyncEventDispatcher::AsyncEventDispatcher(std::unique_ptr<StateManager> stateManager,
                                           std::unique_ptr<BrewProfileManager> brewProfileManager,
                                           std::unique_ptr<UserSettingsManager> userSettingsManager,
                                           std::shared_ptr<BrewHistoryManager> brewHistoryManager,
                                           std::shared_ptr<HistoryRepository> repository)
: m_stateManager{std::move(stateManager)}
, m_brewProfileManager{std::move(brewProfileManager)}
, m_userSettings{std::move(userSettingsManager)}
, m_brewHistoryManager(std::move(brewHistoryManager))
, m_historyRepository{std::move(repository)}
, m_brewCallback{
    m_stateManager->registerBrewProfilerCallback([this](auto cbData) { brewCallback(cbData); }),
  }
{
  m_stateManager->setHeatingStatusChangeCallback([this](auto status) {
    m_executor.executeDiscardResult(&AsyncEventDispatcher::notifyHeatingStatusChange, this, status);
  });
  spdlog::debug("Events dispatcher created");
}

AsyncEventDispatcher::~AsyncEventDispatcher()
{
  m_executor
    .executeWithFuture([this] {
      if (m_stateManager->getBrewState()) {
        m_stateManager->stopBrew();
      }
      m_stateManager->unregisterBrewProfileCallback(m_brewCallback);
      if (m_stateManager->getPowerState()) {
        m_stateManager->setPowerState(false);
      }
    })
    .get();
}

void AsyncEventDispatcher::togglePowerState()
{
  m_executor.executeDiscardResult([this] { doSetPowerState(!m_stateManager->getPowerState()); });
}

void AsyncEventDispatcher::setPowerState(bool state)
{
  m_executor.executeDiscardResult(&AsyncEventDispatcher::doSetPowerState, this, state);
}

void AsyncEventDispatcher::toggleBrew()
{
  m_executor.executeDiscardResult([this] {
    if (m_stateManager->getBrewState()) {
      doStopBrew();
    }
    else {
      doStartBrew();
    }
  });
}

void AsyncEventDispatcher::startBrew()
{
  m_executor.executeDiscardResult(&AsyncEventDispatcher::doStartBrew, this);
}

void AsyncEventDispatcher::stopBrew()
{
  m_executor.executeDiscardResult(&AsyncEventDispatcher::doStopBrew, this);
}

void AsyncEventDispatcher::toggleSteamModeState()
{
  m_executor.executeDiscardResult([this] {
    doSetSteamModeState(!m_stateManager->getSteamModeState());
  });
}

void AsyncEventDispatcher::setSteamModeState(bool state)
{
  m_executor.executeDiscardResult(&AsyncEventDispatcher::doSetSteamModeState, this, state);
}

void AsyncEventDispatcher::addEventsStreamReactor(std::unique_ptr<EventsStreamReactor> reactor)
{
  m_executor.executeDiscardResult([this, reactor = std::move(reactor)] mutable {
    sendInitialState(reactor);
    m_eventsSinks.push_back(std::move(reactor));
  });
}

void AsyncEventDispatcher::doSetPowerState(bool state)
{
  m_stateManager->setPowerState(state);
  PowerState stateChange;
  stateChange.set_value(state);
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(stateChange);
  }
}

void AsyncEventDispatcher::doStartBrew()
{
  if (m_stateManager->getBrewState()) {
    return;
  }

  m_stateManager->startBrew();
  m_brewId = m_historyRepository->createEmptyEntry().get();
  m_brewHistoryManager->startBrew(m_brewId.value(), m_brewProfileManager->getProfile());

  BrewState stateChange;
  stateChange.set_isactive(true);
  stateChange.set_brewid(m_brewId.value());

  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(stateChange);
  }
}

void AsyncEventDispatcher::doStopBrew()
{
  m_brewHistoryManager->stopBrew();

  if (!m_stateManager->getBrewState()) {
    return;
  }
  m_stateManager->stopBrew();

  BrewState state;
  state.set_isactive(false);
  state.set_brewid(m_brewId.value());

  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(state);
  }
}

void AsyncEventDispatcher::notifyHeatingStatusChange(HeatingStatus status)
{
  HeaterState state;
  state.set_value(status);
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(state);
  }
}

void AsyncEventDispatcher::doSetSteamModeState(bool state)
{
  m_stateManager->setSteamModeState(state);
  SteamModeState stateChange;
  stateChange.set_isactive(state);
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(stateChange);
  }
}

void AsyncEventDispatcher::sendInitialState(const std::unique_ptr<EventsStreamReactor>& sink)
{
  {
    PowerState state;
    state.set_value(m_stateManager->getPowerState());
    sink->notifyChanged(state);
  }

  {
    BrewState state;
    state.set_isactive(m_stateManager->getBrewState());
    if (m_brewId.has_value()) {
      state.set_brewid(m_brewId.value());
    }
    sink->notifyChanged(state);
  }

  {
    SteamModeState state;
    state.set_isactive(m_stateManager->getSteamModeState());
    sink->notifyChanged(state);
  }

  {
    HeaterState state;
    state.set_value(m_stateManager->heatingStatus());
    sink->notifyChanged(state);
  }

  {
    ActiveBrewProfile profileInfo;
    profileInfo.mutable_profile()->CopyFrom(m_brewProfileManager->getProfile());
    sink->notifyChanged(profileInfo);
  }

  sink->notifyChanged(m_userSettings->getSettings());
}

void AsyncEventDispatcher::doNotifyBrewStepChange(step_index_t step)
{
  spdlog::debug("Proceed to brew step {}", step);

  BrewProgress progress;
  progress.set_brewstepindex(step);
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(progress);
  }
}

void AsyncEventDispatcher::releaseEventsStreamReactor(const EventsStreamReactor* reactor)
{
  m_executor.executeDiscardResult([this, reactor] {
    m_eventsSinks.remove_if([reactor](auto&& val) { return val.get() == reactor; });
  });
}

void AsyncEventDispatcher::brewCallback(std::variant<step_index_t, stopped_flag_t> step)
{
  auto visitor = overload{
    [this](step_index_t step) {
      m_brewHistoryManager->setCurrentStep(step);
      m_executor.executeDiscardResult(&AsyncEventDispatcher::doNotifyBrewStepChange, this, step);
    },
    [this](stopped_flag_t) {
      m_executor.executeDiscardResult(&AsyncEventDispatcher::doStopBrew, this);
    },
  };

  std::visit(visitor, step);
}

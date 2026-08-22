#include "async_events_dispatcher.hpp"

#include "service/events_stream_reactor.hpp"
#include "state_manager/brew_profile_manager.hpp"
#include "state_manager/state_manager.hpp"
#include "state_manager/user_settings_manager.hpp"
#include "utils/lambda_overload.hpp"

#include <memory>
#include <utility>
#include <variant>

#include <openpresso_proto/openpresso.pb.h>
#include <spdlog/spdlog.h>

using namespace openpressod;

AsyncEventDispatcher::AsyncEventDispatcher(std::unique_ptr<StateManager> stateManager,
                                           std::unique_ptr<BrewProfileManager> brewProfileManager,
                                           std::unique_ptr<UserSettingsManager> userSettingsManager)
: m_stateManager{std::move(stateManager)}
, m_brewProfileManager{std::move(brewProfileManager)}
, m_userSettings{std::move(userSettingsManager)}
, m_brewCallback{
    m_stateManager->registerBrewProfilerCallback([this](auto cbData) { brewCallback(cbData); })}
{
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
      m_stateManager->stopBrew();
    }
    else {
      m_stateManager->startBrew();
    }
  });
}

void AsyncEventDispatcher::startBrew()
{
  m_executor.executeDiscardResult(&StateManager::startBrew, m_stateManager.get());
}

void openpressod::AsyncEventDispatcher::stopBrew()
{
  m_executor.executeDiscardResult(&StateManager::stopBrew, m_stateManager.get());
}

void openpressod::AsyncEventDispatcher::toggleSteamModeState()
{
  m_executor.executeDiscardResult([this] {
    doSetSteamModeState(!m_stateManager->getSteamModeState());
  });
}

void openpressod::AsyncEventDispatcher::setSteamModeState(bool state)
{
  m_executor.executeDiscardResult(&AsyncEventDispatcher::doSetSteamModeState, this, state);
}

void openpressod::AsyncEventDispatcher::addEventsStreamReactor(std::unique_ptr<EventsStreamReactor> reactor)
{
  m_executor.executeDiscardResult([this, reactor = std::move(reactor)] mutable {
    m_eventsSinks.push_back(std::move(reactor));
  });
}

void openpressod::AsyncEventDispatcher::doSetPowerState(bool state)
{
  m_stateManager->setPowerState(state);
  PowerState stateChange;
  stateChange.set_value(state);
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(stateChange);
  }
}

void openpressod::AsyncEventDispatcher::doSetSteamModeState(bool state)
{
  m_stateManager->setSteamModeState(state);
  SteamModeState stateChange;
  stateChange.set_isactive(state);
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(stateChange);
  }
}

void AsyncEventDispatcher::doProcessBrewProfilerStop()
{
  spdlog::debug("Brew profiler stoppped");
  m_stateManager->stopBrew();
  BrewProgress noProgress;
  for (auto&& sink : m_eventsSinks) {
    sink->notifyChanged(noProgress);
  }
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
      m_executor.executeDiscardResult(&AsyncEventDispatcher::doNotifyBrewStepChange, this, step);
    },
    [this](stopped_flag_t) {
      m_executor.executeDiscardResult(&AsyncEventDispatcher::doProcessBrewProfilerStop, this);
    },
  };

  std::visit(visitor, step);
}

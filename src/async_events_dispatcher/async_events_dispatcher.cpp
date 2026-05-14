#include "async_events_dispatcher.hpp"

#include "service/events_stream_reactor.hpp"
#include "state_manager/brew_profile_manager.hpp"
#include "state_manager/state_manager.hpp"
#include "state_manager/user_settings_manager.hpp"

#include <functional>
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
, m_brewCallback{m_stateManager->registerBrewProfilerCallback(
    std::bind_front(&AsyncEventDispatcher::brewCallback, this))}
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
  m_executor.executeDiscardResult([this] {
    m_stateManager->setPowerState(!m_stateManager->getPowerState());
  });
}

void AsyncEventDispatcher::setPowerState(bool state)
{
  m_executor.executeDiscardResult(&StateManager::setPowerState, m_stateManager.get(), state);
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
  m_executor.executeDiscardResult([this] {
    m_stateManager->stopBrew();
    BrewProgress noProgress;
    for (auto&& sink : m_eventsSinks) {
      sink->notifyChanged(noProgress);
    }
  });
}

void openpressod::AsyncEventDispatcher::toggleSteamModeState()
{
  m_executor.executeDiscardResult([this] {
    m_stateManager->setSteamModeState(!m_stateManager->getSteamModeState());
  });
}

void openpressod::AsyncEventDispatcher::setSteamModeState(bool state)
{
  m_executor.executeDiscardResult(&StateManager::setSteamModeState, m_stateManager.get(), state);
}

void openpressod::AsyncEventDispatcher::addEventsStreamReactor(std::unique_ptr<EventsStreamReactor> reactor)
{
  m_executor.executeDiscardResult([this, reactor = std::move(reactor)] mutable {
    m_eventsSinks.push_back(std::move(reactor));
  });
}

void AsyncEventDispatcher::releaseEventsStreamReactor(const EventsStreamReactor* reactor)
{
  m_executor.executeDiscardResult([this, reactor] {
    m_eventsSinks.remove_if([reactor](auto&& val) { return val.get() == reactor; });
  });
}

void openpressod::AsyncEventDispatcher::brewCallback(std::variant<step_index_t, stopped_flag_t> step)
{
  if (std::holds_alternative<stopped_flag_t>(step)) {
    stopBrew();
  }
  else {
    m_executor.executeDiscardResult([this, index = std::get<step_index_t>(step)] {
      spdlog::debug("Process to brew step {}", index);

      BrewProgress progress;
      progress.set_brewstepindex(index);
      for (auto&& sink : m_eventsSinks) {
        sink->notifyChanged(progress);
      }
    });
  }
}

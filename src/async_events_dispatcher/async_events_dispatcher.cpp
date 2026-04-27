#include "async_events_dispatcher.hpp"

#include "state_manager/state_manager.hpp"

#include <functional>
#include <memory>
#include <utility>
#include <variant>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>

using namespace openpressod;

AsyncEventDispatcher::AsyncEventDispatcher(std::unique_ptr<StateManager>&& stateManager)
: m_stateManager{std::move(stateManager)}
, m_brewCallback{m_stateManager->registerBrewProfilerCallback(
    std::bind_front(&AsyncEventDispatcher::brewCallback, this))}
{
}

AsyncEventDispatcher::~AsyncEventDispatcher()
{
  m_executor
    .executeWithFuture([this] {
      m_stateManager->stopBrew();
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
  m_executor.executeDiscardResult(&StateManager::stopBrew, m_stateManager.get());
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
  m_executor.executeDiscardResult(&StateManager::addEventsStreamReactor,
                                  m_stateManager.get(),
                                  std::move(reactor));
}

void openpressod::AsyncEventDispatcher::brewCallback(std::variant<step_index_t, stopped_flag_t> step)
{
  if (std::holds_alternative<stopped_flag_t>(step)) {
    stopBrew();
  }
  else {
    m_executor.executeDiscardResult(&StateManager::onBrewStepChange,
                                    m_stateManager.get(),
                                    std::get<step_index_t>(step));
  }
}

void AsyncEventDispatcher::releaseEventsStreamReactor(const EventsStreamReactor* reactor)
{
  m_executor.executeDiscardResult(&StateManager::releaseEventsStreamReactor, m_stateManager.get(), reactor);
}

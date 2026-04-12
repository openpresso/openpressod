#include "async_events_dispatcher.hpp"

#include "state_manager/state_manager.hpp"

#include <memory>
#include <utility>

using namespace openpressod;

AsyncEventDispatcher::AsyncEventDispatcher(std::unique_ptr<StateManager>&& stateManager)
: m_stateManager(std::move(stateManager))
{
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

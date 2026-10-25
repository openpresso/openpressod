#include "brew_log_manager.hpp"

#include "config/openpressod_config.hpp"
#include "service/brew_log_stream_reactor.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <ranges>
#include <utility>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/types.hpp>

using namespace openpressod;

BrewLogManager::BrewLogManager(const libopenpresso::CorePtr& core, const OpenpressodConfig& config)
: m_samplingInterval{config.brewLogSamplingInterval()}
, m_sampler{core, m_samplingInterval}
{
}

BrewLogManager::~BrewLogManager()
{
  m_sampler.stopSampling();
}

void BrewLogManager::addStreamReactor(std::unique_ptr<BrewLogStreamReactor> reactor)
{
  m_executor.executeDiscardResult(&BrewLogManager::doAddStreamReactor, this, std::move(reactor));
}

void BrewLogManager::removeStreamReactor(const BrewLogStreamReactor* reactor)
{
  m_executor.executeDiscardResult(&BrewLogManager::doRemoveStreamReactor, this, reactor);
}

void BrewLogManager::startBrew()
{
  m_executor.executeDiscardResult(&BrewLogManager::doStartBrew, this);
  m_sampler.startSampling([this](auto... metrics) {
    m_executor.executeDiscardResult(&BrewLogManager::doPushData, this, metrics...);
  });
}

void BrewLogManager::setCurrentStep(size_t step)
{
  m_currentStep.store(step, std::memory_order_relaxed);
}

void BrewLogManager::stopBrew()
{
  m_sampler.stopSampling();
  m_executor.executeDiscardResult(&BrewLogManager::doStopBrew, this);
}

void BrewLogManager::resetLog()
{
  m_executor.executeDiscardResult(&BrewLogManager::doResetLog, this);
}

void BrewLogManager::doAddStreamReactor(std::unique_ptr<BrewLogStreamReactor> reactor)
{
  m_eventsSinks.push_back(std::move(reactor));
  if (!m_records.empty() || m_isBrewing) {
    m_eventsSinks.back()->sendBrewStart(m_samplingInterval, m_brewStartTime);
    m_eventsSinks.back()->sendRecords(m_records);
    if (!m_isBrewing) {
      m_eventsSinks.back()->sendBrewStop();
    }
  }
}

void BrewLogManager::doRemoveStreamReactor(const BrewLogStreamReactor* reactor)
{
  m_eventsSinks.remove_if([reactor](auto&& val) { return val.get() == reactor; });
}

void BrewLogManager::doStartBrew()
{
  m_records.clear();
  m_currentStep.store(0, std::memory_order_relaxed);
  m_isBrewing = true;
  m_brewStartTime = std::chrono::system_clock::now();
  for (auto&& sink : m_eventsSinks) {
    sink->sendBrewStart(m_samplingInterval, m_brewStartTime);
  }
}

void BrewLogManager::doStopBrew()
{
  if (!m_isBrewing) {
    return;
  }

  for (auto&& sink : m_eventsSinks) {
    sink->sendBrewStop();
  }
  m_isBrewing = false;
}

void BrewLogManager::doResetLog()
{
  m_records.clear();
  for (auto&& sink : m_eventsSinks) {
    sink->sendLogCleanUp();
  }
}

void BrewLogManager::doPushData(libopenpresso::millidegrees_t temperature,
                                libopenpresso::millibars_t pressure,
                                libopenpresso::milligrams_t weight,
                                libopenpresso::milligrams_p_second_t flowRate)
{
  BrewLogRecord record{
    .temperature = temperature,
    .pressure = pressure,
    .weight = weight,
    .flowRate = flowRate,
    .step = m_currentStep.load(std::memory_order_relaxed),
  };

  for (auto&& sink : m_eventsSinks) {
    sink->sendRecords(std::ranges::single_view{record});
  }

  m_records.push_back(record);
}

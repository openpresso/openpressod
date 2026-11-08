#include "brew_log_manager.hpp"

#include "brew_log_data.hpp"
#include "config/openpressod_config.hpp"
#include "database/brew_history/async_repository.hpp" // IWYU pragma: keep
#include "service/brew_log_stream_reactor.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <utility>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/types.hpp>
#include <openpresso_proto/brew_profile.pb.h>

using namespace openpressod;

BrewLogManager::BrewLogManager(const libopenpresso::CorePtr& core,
                               std::shared_ptr<Repository> repository,
                               const OpenpressodConfig& config)
: m_repository{std::move(repository)}
, m_samplingInterval{config.brewLogSamplingInterval()}
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

void BrewLogManager::startBrew(uint32_t brewId, BrewProfile profile)
{
  m_executor.executeDiscardResult(&BrewLogManager::doStartBrew, this, brewId, std::move(profile));
  m_sampler.startSampling([this](auto... metrics) {
    m_executor.executeDiscardResult(&BrewLogManager::doPushData, this, metrics...);
  });
}

void BrewLogManager::setCurrentStep(size_t step)
{
  m_sampler.setCurrentStep(step);
}

void BrewLogManager::stopBrew()
{
  m_sampler.stopSampling();
  m_executor.executeDiscardResult(&BrewLogManager::doStopBrew, this);
}

void BrewLogManager::doAddStreamReactor(std::unique_ptr<BrewLogStreamReactor> reactor)
{
  if (m_brewId.has_value() && reactor->brewId() == m_brewId.value()) {
    reactor->sendBrewStart(m_samplingInterval, m_brewStartTime, m_profile, m_records);
    m_activeEventSinks.push_back(std::move(reactor));
    return;
  }

  m_repository->get(reactor->brewId(), [reactor = reactor.get()](auto&& brewLogFuture) {
    try {
      auto&& [log, profile] = brewLogFuture.get();
      reactor->sendCompletedBrew(log.samplingInterval, log.brewStartTime, std::move(profile), log.records);
    }
    catch (const std::system_error&) {
      reactor->sendLogNotFound();
    }
  });

  m_eventsSinks.push_back(std::move(reactor));
}

void BrewLogManager::doRemoveStreamReactor(const BrewLogStreamReactor* reactor)
{
  if (m_brewId.has_value() && reactor->brewId() == m_brewId.value()) {
    m_activeEventSinks.remove_if([reactor](auto&& val) { return val.get() == reactor; });
  }
  else {
    m_eventsSinks.remove_if([reactor](auto&& val) { return val.get() == reactor; });
  }
}

void BrewLogManager::doStartBrew(uint32_t brewId, BrewProfile profile)
{
  m_records.clear();
  m_brewStartTime = std::chrono::system_clock::now();
  m_brewId = brewId;
  m_profile = std::move(profile);
}

void BrewLogManager::doStopBrew()
{
  if (!m_brewId.has_value()) {
    return;
  }

  for (auto&& sink : m_activeEventSinks) {
    sink->sendBrewStop();
  }
  m_eventsSinks.splice(m_eventsSinks.end(), m_activeEventSinks);
  m_brewId.reset();

  CompletedBrewLog log{
    .brewStartTime = m_brewStartTime,
    .samplingInterval = m_samplingInterval,
    .records = std::move(m_records),
  };

  // m_brewLogsDb.store(std::move(log));
}

void BrewLogManager::doPushData(libopenpresso::millibars_t pressure,
                                libopenpresso::milligrams_t weight,
                                libopenpresso::milligrams_p_second_t flowRate,
                                size_t stepIndex)
{
  BrewLogRecord record{
    .pressure = pressure,
    .weight = weight,
    .flowRate = flowRate,
    .step = stepIndex,
  };

  for (auto&& sink : m_activeEventSinks) {
    sink->sendRecord(record);
  }

  m_records.push_back(record);
}

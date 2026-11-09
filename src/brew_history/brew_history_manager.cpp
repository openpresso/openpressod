#include "brew_history_manager.hpp"

#include "brew_history_data.hpp"
#include "config/openpressod_config.hpp"
#include "database/brew_history/async_repository.hpp" // IWYU pragma: keep
#include "database/database_id.hpp"
#include "service/brew_history_stream_reactor.hpp"

#include <chrono>
#include <cstddef>
#include <memory>
#include <system_error>
#include <utility>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/types.hpp>
#include <openpresso_proto/brew_profile.pb.h>

using namespace openpressod;

BrewHistoryManager::BrewHistoryManager(const libopenpresso::CorePtr& core,
                                       std::shared_ptr<Repository> repository,
                                       const OpenpressodConfig& config)
: m_repository{std::move(repository)}
, m_samplingInterval{config.brewHistorySamplingInterval()}
, m_sampler{core, m_samplingInterval}
{
}

BrewHistoryManager::~BrewHistoryManager()
{
  m_sampler.stopSampling();
}

void BrewHistoryManager::addStreamReactor(std::unique_ptr<BrewHistoryStreamReactor> reactor)
{
  m_executor.executeDiscardResult(&BrewHistoryManager::doAddStreamReactor, this, std::move(reactor));
}

void BrewHistoryManager::removeStreamReactor(const BrewHistoryStreamReactor* reactor)
{
  m_executor.executeDiscardResult(&BrewHistoryManager::doRemoveStreamReactor, this, reactor);
}

void BrewHistoryManager::startBrew(DatabaseId brewId, BrewProfile profile)
{
  m_executor.executeDiscardResult(&BrewHistoryManager::doStartBrew, this, brewId, std::move(profile));
  m_sampler.startSampling([this](auto... metrics) {
    m_executor.executeDiscardResult(&BrewHistoryManager::doPushSample, this, metrics...);
  });
}

void BrewHistoryManager::setCurrentStep(size_t step)
{
  m_sampler.setCurrentStep(step);
}

void BrewHistoryManager::stopBrew()
{
  m_sampler.stopSampling();
  m_executor.executeDiscardResult(&BrewHistoryManager::doStopBrew, this);
}

void BrewHistoryManager::doAddStreamReactor(std::unique_ptr<BrewHistoryStreamReactor> reactor)
{
  if (m_brewId.has_value() && reactor->brewId() == m_brewId.value()) {
    reactor->sendBrewStart(m_samplingInterval, m_brewStartTime, m_profile, m_samples);
    m_activeEventSinks.push_back(std::move(reactor));
    return;
  }

  m_repository->get(reactor->brewId(), [reactor = reactor.get()](auto&& entryFuture) {
    try {
      auto entry = entryFuture.get();
      reactor->sendCompletedBrew(
        entry.samplingInterval, entry.brewStartTime, std::move(entry.profile), entry.samples);
    }
    catch (const std::system_error&) {
      reactor->sendEntryNotFound();
    }
  });

  m_eventsSinks.push_back(std::move(reactor));
}

void BrewHistoryManager::doRemoveStreamReactor(const BrewHistoryStreamReactor* reactor)
{
  if (m_brewId.has_value() && reactor->brewId() == m_brewId.value()) {
    m_activeEventSinks.remove_if([reactor](auto&& val) { return val.get() == reactor; });
  }
  else {
    m_eventsSinks.remove_if([reactor](auto&& val) { return val.get() == reactor; });
  }
}

void BrewHistoryManager::doStartBrew(DatabaseId brewId, BrewProfile profile)
{
  m_samples.clear();
  m_brewStartTime = std::chrono::system_clock::now();
  m_brewId = brewId;
  m_profile = std::move(profile);
}

void BrewHistoryManager::doStopBrew()
{
  if (!m_brewId.has_value()) {
    return;
  }

  for (auto&& sink : m_activeEventSinks) {
    sink->sendBrewStop();
  }
  m_eventsSinks.splice(m_eventsSinks.end(), m_activeEventSinks);

  CompletedBrewHistoryEntry entry{
    .brewStartTime = m_brewStartTime,
    .samplingInterval = m_samplingInterval,
    .profile = std::move(m_profile),
    .samples = std::move(m_samples),
  };

  m_repository->completeEntry(m_brewId.value(), std::move(entry));
  m_brewId.reset();
}

void BrewHistoryManager::doPushSample(libopenpresso::millibars_t pressure,
                                      libopenpresso::milligrams_t weight,
                                      libopenpresso::milligrams_p_second_t flowRate,
                                      size_t stepIndex)
{
  BrewHistoryEntrySampleData sample{
    .pressure = pressure,
    .weight = weight,
    .flowRate = flowRate,
    .step = stepIndex,
  };

  for (auto&& sink : m_activeEventSinks) {
    sink->sendSample(sample);
  }

  m_samples.push_back(sample);
}

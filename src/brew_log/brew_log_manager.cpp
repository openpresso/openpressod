#include "brew_log_manager.hpp"

#include "config/libopenpresso_config_labels.hpp"
#include "config/openpressod_config.hpp"
#include "service/brew_log_stream_reactor.hpp"
#include "utils/lambda_overload.hpp"

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <ranges>
#include <utility>
#include <variant>

#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/types.hpp>

using namespace openpressod;

BrewLogManager::BrewLogManager(const libopenpresso::CorePtr& core, const OpenpressodConfig& config)
: m_samplingInterval{config.brewLogSamplingInterval()}
, m_sampler{core, m_samplingInterval}
, m_profilerCallback{core->getBrewProfiler(libopenpresso_config_labels::BREW_PROFILER_LABEL)
                       ->registerStepChangeCallback([this](auto step) { brewProfilerCallback(step); })}
{
}

BrewLogManager::~BrewLogManager()
{
  m_profilerCallback.reset();
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

void BrewLogManager::pushData(libopenpresso::millidegrees_t temperature,
                              libopenpresso::millibars_t pressure,
                              libopenpresso::milligrams_t weight,
                              libopenpresso::milligrams_p_second_t flowRate)
{
  m_executor.executeDiscardResult(&BrewLogManager::doPushData, this, temperature, pressure, weight, flowRate);
}

void BrewLogManager::doAddStreamReactor(std::unique_ptr<BrewLogStreamReactor> reactor)
{
  m_eventsSinks.push_back(std::move(reactor));
  if (!m_records.empty()) {
    m_eventsSinks.back()->sendBrewStart(m_samplingInterval, m_brewStartTime);
    m_eventsSinks.back()->sendRecords(m_records);
  }
}

void BrewLogManager::doRemoveStreamReactor(const BrewLogStreamReactor* reactor)
{
  m_eventsSinks.remove_if([reactor](auto&& val) { return val.get() == reactor; });
}

void BrewLogManager::doStartBrew()
{
  m_records.clear();
  m_currentStep = 0;
  m_brewStartTime = std::chrono::system_clock::now();
}

void BrewLogManager::doAdvanceStep(size_t index)
{
  m_currentStep = index;
}

void BrewLogManager::doPushData(libopenpresso::millidegrees_t temperature,
                                libopenpresso::millibars_t pressure,
                                libopenpresso::milligrams_t weight,
                                libopenpresso::milligrams_p_second_t flowRate)
{
  if (m_records.empty()) {
    for (auto&& sink : m_eventsSinks) {
      sink->sendBrewStart(m_samplingInterval, m_brewStartTime);
    }
  }

  BrewLogRecord record{
    .temperature = temperature,
    .pressure = pressure,
    .weight = weight,
    .flowRate = flowRate,
    .step = m_currentStep,
  };

  for (auto&& sink : m_eventsSinks) {
    sink->sendRecords(std::ranges::single_view{record});
  }
  m_records.push_back(record);
}

void BrewLogManager::brewProfilerCallback(std::variant<step_index_t, stopped_flag_t> step)
{
  auto stepVisitor = [this](step_index_t step) {
    if (step == 0) {
      m_executor.executeDiscardResult(&BrewLogManager::doStartBrew, this);
      m_sampler.startSampling(std::bind_front(&BrewLogManager::doPushData, this));
    }
    else {
      m_executor.executeDiscardResult(&BrewLogManager::doAdvanceStep, this, step);
    }
  };
  auto stopVisitor = [this](stopped_flag_t) { m_sampler.stopSampling(); };

  std::visit(
    overload{
      std::move(stepVisitor),
      std::move(stopVisitor),
    },
    step);
}

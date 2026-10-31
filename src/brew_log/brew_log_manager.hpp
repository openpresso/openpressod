#ifndef BREW_LOG_MANAGER_HPP
#define BREW_LOG_MANAGER_HPP

#include "brew_log_sampler.hpp"
#include "utils/procedures_serial_executor.hpp"

#include <chrono>
#include <cstddef>
#include <list>
#include <memory>
#include <vector>

#include <libopenpresso/interfaces/brew_profiler.hpp>
#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/types.hpp>

namespace openpressod
{

class BrewLogStreamReactor;
class OpenpressodConfig;

class BrewLogManager {
  using step_index_t = libopenpresso::interfaces::BrewProfiler::step_index_t;
  using stopped_flag_t = libopenpresso::interfaces::BrewProfiler::stopped_flag_t;

  struct BrewLogRecord {
    libopenpresso::millidegrees_t temperature;
    libopenpresso::millibars_t pressure;
    libopenpresso::milligrams_t weight;
    libopenpresso::milligrams_p_second_t flowRate;
    size_t step;
  };

  struct CompletedBrewLog {
    std::chrono::system_clock::time_point brewStartTime;
    std::chrono::steady_clock::duration samplingInterval;
    std::vector<BrewLogRecord> records;
  };

public:
  BrewLogManager(const libopenpresso::CorePtr& core, const OpenpressodConfig& config);
  BrewLogManager(const BrewLogManager&) = delete;
  BrewLogManager(BrewLogManager&&) = delete;
  auto operator=(const BrewLogManager&) = delete;
  auto operator=(BrewLogManager&&) = delete;
  ~BrewLogManager();

  void addStreamReactor(std::unique_ptr<BrewLogStreamReactor> reactor);
  void removeStreamReactor(const BrewLogStreamReactor* reactor);

  void startBrew(uint32_t brewId);
  void setCurrentStep(size_t step);
  void stopBrew();

private:
  void doAddStreamReactor(std::unique_ptr<BrewLogStreamReactor> reactor);
  void doRemoveStreamReactor(const BrewLogStreamReactor* reactor);

  void doStartBrew(uint32_t brewId);
  void doStopBrew();

  void doPushData(libopenpresso::millidegrees_t temperature,
                  libopenpresso::millibars_t pressure,
                  libopenpresso::milligrams_t weight,
                  libopenpresso::milligrams_p_second_t flowRate,
                  size_t stepIndex);

private:
  size_t m_maxBrewLogsCount;
  std::list<std::pair<uint32_t, CompletedBrewLog>> m_completedLogs;

  std::chrono::system_clock::time_point m_brewStartTime;
  std::chrono::steady_clock::duration m_samplingInterval;
  std::optional<uint32_t> m_brewId;
  std::vector<BrewLogRecord> m_records;

  std::list<std::unique_ptr<BrewLogStreamReactor>> m_eventsSinks;
  std::list<std::unique_ptr<BrewLogStreamReactor>> m_activeEventSinks;
  ProceduresSerialExecutor m_executor;
  BrewLogSampler m_sampler;
};

} // namespace openpressod

#endif // BREW_LOG_MANAGER_HPP

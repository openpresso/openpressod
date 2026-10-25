#ifndef BREW_LOG_MANAGER_HPP
#define BREW_LOG_MANAGER_HPP

#include "brew_log_sampler.hpp"
#include "utils/procedures_serial_executor.hpp"

#include <atomic>
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

public:
  BrewLogManager(const libopenpresso::CorePtr& core, const OpenpressodConfig& config);
  BrewLogManager(const BrewLogManager&) = delete;
  BrewLogManager(BrewLogManager&&) = delete;
  auto operator=(const BrewLogManager&) = delete;
  auto operator=(BrewLogManager&&) = delete;
  ~BrewLogManager();

  void addStreamReactor(std::unique_ptr<BrewLogStreamReactor> reactor);
  void removeStreamReactor(const BrewLogStreamReactor* reactor);

  void startBrew();
  void setCurrentStep(size_t step);
  void stopBrew();
  void resetLog();

private:
  void doAddStreamReactor(std::unique_ptr<BrewLogStreamReactor> reactor);
  void doRemoveStreamReactor(const BrewLogStreamReactor* reactor);

  void doStartBrew();
  void doStopBrew();
  void doResetLog();

  void doPushData(libopenpresso::millidegrees_t temperature,
                  libopenpresso::millibars_t pressure,
                  libopenpresso::milligrams_t weight,
                  libopenpresso::milligrams_p_second_t flowRate);

private:
  std::chrono::system_clock::time_point m_brewStartTime;
  std::chrono::steady_clock::duration m_samplingInterval;
  bool m_isBrewing = false;
  std::atomic<size_t> m_currentStep = 0;
  std::vector<BrewLogRecord> m_records;
  std::list<std::unique_ptr<BrewLogStreamReactor>> m_eventsSinks;
  ProceduresSerialExecutor m_executor;
  BrewLogSampler m_sampler;
};

} // namespace openpressod

#endif // BREW_LOG_MANAGER_HPP

#ifndef BREW_HISTORY_MANAGER_HPP
#define BREW_HISTORY_MANAGER_HPP

#include "brew_history_data.hpp"
#include "brew_history_sampler.hpp"
#include "database/database_id.hpp"
#include "utils/procedures_serial_executor.hpp"

#include <chrono>
#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <vector>

#include <libopenpresso/interfaces/brew_profiler.hpp>
#include <libopenpresso/interfaces/libopenpresso_core.hpp>
#include <libopenpresso/types.hpp>
#include <openpresso_proto/brew_profile.pb.h>

namespace openpressod
{

class BrewHistoryStreamReactor;
class OpenpressodConfig;

namespace db::brew_history
{
class AsyncBrewHistoryRepository;
}

class BrewHistoryManager {
  using step_index_t = libopenpresso::interfaces::BrewProfiler::step_index_t;
  using stopped_flag_t = libopenpresso::interfaces::BrewProfiler::stopped_flag_t;
  using Repository = db::brew_history::AsyncBrewHistoryRepository;

public:
  BrewHistoryManager(const libopenpresso::CorePtr& core,
                     std::shared_ptr<Repository> repository,
                     const OpenpressodConfig& config);
  BrewHistoryManager(const BrewHistoryManager&) = delete;
  BrewHistoryManager(BrewHistoryManager&&) = delete;
  auto operator=(const BrewHistoryManager&) = delete;
  auto operator=(BrewHistoryManager&&) = delete;
  ~BrewHistoryManager();

  void addStreamReactor(std::unique_ptr<BrewHistoryStreamReactor> reactor);
  void removeStreamReactor(const BrewHistoryStreamReactor* reactor);

  void startBrew(DatabaseId brewId, BrewProfile profile);
  void setCurrentStep(size_t step);
  void stopBrew();

private:
  void doAddStreamReactor(std::unique_ptr<BrewHistoryStreamReactor> reactor);
  void doRemoveStreamReactor(const BrewHistoryStreamReactor* reactor);

  void doStartBrew(DatabaseId brewId, BrewProfile profile);
  void doStopBrew();

  void doPushSample(libopenpresso::millibars_t pressure,
                    libopenpresso::milligrams_t weight,
                    libopenpresso::milligrams_p_second_t flowRate,
                    size_t stepIndex);

private:
  std::shared_ptr<Repository> m_repository;

  std::chrono::system_clock::time_point m_brewStartTime;
  std::chrono::steady_clock::duration m_samplingInterval;
  std::optional<DatabaseId> m_brewId;
  BrewProfile m_profile;
  std::vector<BrewHistoryEntrySampleData> m_samples;

  std::list<std::unique_ptr<BrewHistoryStreamReactor>> m_eventsSinks;
  std::list<std::unique_ptr<BrewHistoryStreamReactor>> m_activeEventSinks;
  ProceduresSerialExecutor m_executor;
  BrewHistorySampler m_sampler;
};

} // namespace openpressod

#endif // BREW_HISTORY_MANAGER_HPP

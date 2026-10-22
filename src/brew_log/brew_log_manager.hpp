#ifndef BREW_LOG_MANAGER_HPP
#define BREW_LOG_MANAGER_HPP

#include "utils/procedures_serial_executor.hpp"

#include <chrono>
#include <list>
#include <memory>
#include <vector>

#include <libopenpresso/types.hpp>

namespace openpressod
{

class BrewLogStreamReactor;
class OpenpressodConfig;

class BrewLogManager {
  struct BrewLogRecord {
    libopenpresso::millidegrees_t temperature;
    libopenpresso::millibars_t pressure;
    libopenpresso::milligrams_t weight;
    libopenpresso::milligrams_p_second_t flowRate;
    size_t step;
  };

public:
  BrewLogManager(const OpenpressodConfig& config);

  void addStreamReactor(std::unique_ptr<BrewLogStreamReactor> reactor);
  void removeStreamReactor(const BrewLogStreamReactor* reactor);

  void setStepIndex(size_t step);

  void pushData(libopenpresso::millidegrees_t temperature,
                libopenpresso::millibars_t pressure,
                libopenpresso::milligrams_t weight,
                libopenpresso::milligrams_p_second_t flowRate);

private:
  void doAddStreamReactor(std::unique_ptr<BrewLogStreamReactor> reactor);
  void doRemoveStreamReactor(const BrewLogStreamReactor* reactor);

  void doStartBrew();
  void doAdvanceStep(size_t index);

  void doPushData(libopenpresso::millidegrees_t temperature,
                  libopenpresso::millibars_t pressure,
                  libopenpresso::milligrams_t weight,
                  libopenpresso::milligrams_p_second_t flowRate);

private:
  std::chrono::system_clock::time_point m_brewStartTime;
  std::chrono::steady_clock::duration m_samplingInterval;
  size_t m_currentStep = 0;
  std::vector<BrewLogRecord> m_records;
  std::list<std::unique_ptr<BrewLogStreamReactor>> m_eventsSinks;
  ProceduresSerialExecutor m_executor;
};

} // namespace openpressod

#endif // BREW_LOG_MANAGER_HPP
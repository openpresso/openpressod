#ifndef BREW_LOG_DATA_HPP
#define BREW_LOG_DATA_HPP

#include <chrono>
#include <cstddef>
#include <vector>

#include <libopenpresso/types.hpp>

namespace openpressod
{

struct BrewLogRecord {
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

} // namespace openpressod

#endif // BREW_LOG_DATA_HPP

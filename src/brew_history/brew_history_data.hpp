#ifndef BREW_HISTORY_DATA_HPP
#define BREW_HISTORY_DATA_HPP

#include <chrono>
#include <cstddef>
#include <vector>

#include <libopenpresso/types.hpp>
#include <openpresso_proto/brew_profile.pb.h>

namespace openpressod
{

struct BrewHistoryEntrySampleData {
  libopenpresso::millibars_t pressure;
  libopenpresso::milligrams_t weight;
  libopenpresso::milligrams_p_second_t flowRate;
  size_t step;
};

struct CompletedBrewHistoryEntry {
  std::chrono::system_clock::time_point brewStartTime;
  std::chrono::steady_clock::duration samplingInterval;
  BrewProfile profile;
  std::vector<BrewHistoryEntrySampleData> samples;
};

} // namespace openpressod

#endif // BREW_HISTORY_DATA_HPP

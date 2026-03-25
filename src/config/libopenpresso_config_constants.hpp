#ifndef LIBOPENPRESSO_CONFIG_CONSTANTS_HPP
#define LIBOPENPRESSO_CONFIG_CONSTANTS_HPP

#include <chrono> // IWYU pragma: keep
#include <cstddef>

#include <libopenpresso/types.hpp>

using namespace std::chrono_literals;

namespace openpressod::libopenpresso_config_constants
{

static constexpr auto BREW_PROFILER_STATE_UPDATE_PERIOD = 50ms;     // NOLINT(misc-include-cleaner)
static constexpr auto STEAM_CONTROLLER_STATE_UPDATE_PERIOD = 250ms; // NOLINT(misc-include-cleaner)

static constexpr size_t PUMP_POWER_DUTY_CYCLE = 16;
static constexpr size_t HEATER_POWER_DUTY_CYCLE = 64;

static constexpr libopenpresso::millidegrees_t STEAM_TEMPERATURE_THRESHOLD_OFFSET = 10'000;

} // namespace openpressod::libopenpresso_config_constants

#endif // LIBOPENPRESSO_CONFIG_CONSTANTS_HPP
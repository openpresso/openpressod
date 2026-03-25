#ifndef CONFIG_DEFAULTS_HPP
#define CONFIG_DEFAULTS_HPP

#include <chrono> // IWYU pragma: keep

#include <libopenpresso/pin_info.hpp>
#include <libopenpresso/types.hpp>
#include <spdlog/common.h>

namespace openpressod::config_defaults
{

using namespace std::chrono_literals;

static constexpr auto DEFAULT_BREW_PROFILE_NAME = "brew_profile.json";
static constexpr auto DEFAULT_SOCKET_PATH = "/var/run/openpresso.sock";

static constexpr auto DEFAULT_I2C_BUS = "/dev/i2c-0";
static constexpr auto DEFAULT_GPIO_CHIP = "/dev/gpiochip0";
static constexpr auto DEFAULT_WATCHDOG = "/dev/watchdog0";
static constexpr auto DEFAULT_TEMPERATURE_SENSOR_SPI = "/dev/spidev0.0";

static constexpr bool DEFAULT_LOGFILE_ENABLE = true;
static constexpr auto DEFAULT_LOGFILES_DIRECTORY_NAME = "logs";
static constexpr auto DEFAULT_LOGFILE_OUTPUT_PATTERN = "%+";
static constexpr auto DEFAULT_CONSOLE_LOG_OUTPUT_PATTERN = "%+";
static constexpr auto DEFAULT_LOGFILE_LEVEL = spdlog::level::info;
static constexpr auto DEFAULT_CONSOLE_LOG_LEVEL = spdlog::level::info;

static constexpr bool DEFAULT_WATCHDOG_ENABLE = true;

static constexpr bool DEFAULT_POWER_BUTTON_BISTABLE = false;
static constexpr bool DEFAULT_BREW_BUTTON_BISTABLE = true;
static constexpr bool DEFAULT_STEAM_BUTTON_BISTABLE = true;

static constexpr bool DEFAULT_BUTTONS_LOGIC_INVERTED = true;
static constexpr bool DEFAULT_LEDS_LOGIC_INVERTED = false;
static constexpr auto DEFAULT_BUTTONS_PULL_MODE = libopenpresso::PinPull::PullUp;

static constexpr auto DEFAULT_BUTTON_DEBOUNCE_PERIOD = 10ms; // NOLINT(misc-include-cleaner)
static constexpr auto DEFAULT_TEMPERATURE_D_FILTER_SMOOTHING_PERIOD = 750ms; // NOLINT(misc-include-cleaner)
static constexpr auto DEFAULT_WEIGHT_D_FILTER_SMOOTHING_PERIOD = 600ms; // NOLINT(misc-include-cleaner)
static constexpr auto DEFAULT_WATCHDOG_TIMEOUT = 1s; // NOLINT(misc-include-cleaner)

static constexpr libopenpresso::i2c_dev_addr_t DEFAULT_ADS1115_ADDR = 0x48;
static constexpr libopenpresso::i2c_dev_addr_t DEFAULT_NAU7802_ADDR = 0x2a;

static constexpr libopenpresso::millibars_t DEFAULT_BREW_PRESSURE = 9'000;
static constexpr libopenpresso::millidegrees_t DEFAULT_BREW_TEMPERATURE = 95'000;
static constexpr libopenpresso::millidegrees_t DEFAULT_STEAM_TEMPERATURE = 155'000;
static constexpr libopenpresso::millibars_t DEFAULT_STEAM_PRESSURE_THRESHOLD = 2'500;
static constexpr libopenpresso::milligrams_p_second_t DEFAULT_STEAM_REFILL_FLOW = 500;

static constexpr libopenpresso::millidegrees_t DEFAULT_WATCHDOG_RESET_TEMPERATURE = 170'000;

} // namespace openpressod::config_defaults

#endif // CONFIG_DEFAULTS_HPP
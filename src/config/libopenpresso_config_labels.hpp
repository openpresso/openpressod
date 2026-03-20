#ifndef LIBOPENPRESSO_CONFIG_LABELS_HPP
#define LIBOPENPRESSO_CONFIG_LABELS_HPP

namespace openpressod::libopenpresso_config_labels
{

static constexpr auto PRESSURE_SENSOR_LABEL = "pressure_sensor";
static constexpr auto TEMPERATURE_SENSOR_LABEL = "temperature_sensor";
static constexpr auto AC_SENSOR_LABEL = "ac_sensor";
static constexpr auto WEIGHT_SENSOR_LABEL = "weight_sensor";
static constexpr auto INTERNAL_FLOW_SENSOR_LABEL = "flow_sensor";

static constexpr auto POWER_BUTTON_LABEL = "power_button";
static constexpr auto BREW_BUTTON_LABEL = "brew_button";
static constexpr auto STEAM_BUTTON_LABEL = "steam_button";

static constexpr auto POWER_LED_LABEL = "power_led";
static constexpr auto BREW_LED_LABEL = "brew_led";
static constexpr auto STEAM_LED_LABEL = "steam_led";

static constexpr auto PUMP_DRIVER_LABEL = "pump_driver";
static constexpr auto HEATER_DRIVER_LABEL = "heater_driver";
static constexpr auto VALVE_DRIVER_LABEL = "valve_driver";

static constexpr auto PUMP_POWER_CONTROLLER_LABEL = "pump_power_controller";
static constexpr auto HEATER_POWER_CONTROLLER_LABEL = "heater_power_controller";

static constexpr auto PRESSURE_CONTROLLER_LABEL = "pressure_controller";
static constexpr auto FLOW_RATE_CONTROLLER_LABEL = "flow_rate_controller";
static constexpr auto INTERNAL_FLOW_RATE_CONTROLLER_LABEL = "internal_flow_rate_controller";
static constexpr auto BREW_TEMPERATURE_CONTROLLER_LABEL = "brew_temperature_controller";
static constexpr auto STEAM_PREHEAT_TEMPERATURE_CONTROLLER_LABEL =
  "steam_preheat_temperature_controller";
static constexpr auto STEAM_ACTIVE_TEMPERATURE_CONTROLLER_LABEL =
  "steam_active_temperature_controller";

static constexpr auto BREW_PROFILER_LABEL = "brew_profiler";
static constexpr auto STEAM_CONTROLLER_LABEL = "steam_controller";

} // namespace openpressod::libopenpresso_config_labels

#endif // LIBOPENPRESSO_CONFIG_LABELS_HPP
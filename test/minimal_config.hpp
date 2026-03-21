#ifndef MINIMAL_CONFIG_HPP
#define MINIMAL_CONFIG_HPP

// Minimal config matching config.toml – contains only the required fields
static constexpr auto MINIMAL_CONFIG = R"(
[global]
mains_frequency = 50
mains_zc_pin = 17
valve_control_pin = 22

[buttons]
power_pin = 26
brew_pin = 19
steam_pin = 13

[leds]
power_pin = 21
brew_pin = 16
steam_pin = 20

[pressure_sensor]
signal_pin = 4

[weight_sensor]
scale = 27762
signal_pin = 12

[temperature_sensor]
signal_pin = 25

[pump]
flow_controller_multiplier = 0.00005
control_pin = 27
volume_per_pulse = 205000
stall_pressure = 18500

[heater]
control_pin = 23

[heater.brew_pid_settings]
p = 0.07
d = 0.25
i = 0.0025
d_relax = 0.025
i_relax = 1.0
f = 0.04
w = 0.002
w_decay = 0.02

[steam.preheat_pid_settings]
p = 0.08
d = 0.2
i = 0.01
d_relax = 0.05
i_relax = 1.5

[steam.active_pid_settings]
p = 0.25
d = 0.5
i = 0.02
d_relax = 0.0
i_relax = 0.5
)";

#endif // MINIMAL_CONFIG_HPP

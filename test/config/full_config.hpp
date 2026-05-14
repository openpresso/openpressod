#ifndef FULL_CONFIG_HPP
#define FULL_CONFIG_HPP

// Full config that overrides every optional/default field
static constexpr auto FULL_CONFIG = R"(
[global]
mains_frequency = 60
mains_zc_pin = 5
brew_profile_path = "/custom/profile.json"
socket_path = "/tmp/custom.sock"
i2c_bus = "/dev/i2c-1"
gpio_chip = "/dev/gpiochip4"
user_settings_path = "/custom/user_settings.json"
pid_state_monitoring_enabled = true

[log]
console_log_pattern = "[%l] %v"
console_log_level = "warn"

[watchdog]
enabled = false
timeout = 00:00:05
device_path = "/dev/watchdog1"
temperature_limit = 180000

[buttons]
inverted = false
debounce_period = 00:00:00.002
power_bistable = true
brew_bistable = false
steam_bistable = false
pull_mode = "PullDown"
power_pin = 1
brew_pin = 2
steam_pin = 3

[leds]
inverted = true
power_pin = 10
brew_pin = 11
steam_pin = 12

[pressure_sensor]
signal_pin = 7
i2c_addr = 73

[weight_sensor]
scale = 50000
filter_timing = 00:00:00.5
signal_pin = 8
i2c_addr = 30

[temperature_sensor]
spi_addr = "/dev/spidev1.0"
filter_timing = 00:00:00.150
signal_pin = 9

[pump]
flow_controller_multiplier = 0.0001
control_pin = 14
volume_per_pulse = 300000
stall_pressure = 20000

[heater]
control_pin = 15

[valve]
control_pin = 6

[brew]
temperature = 92_000
pressure = 8_500
brew_temperature_window = 200

[brew.pid_settings]
p = 0.1
d = 0.3
i = 0.005
d_relax = 0.03
i_relax = 1.5
f = 0.06
w = 0.003
w_decay = 0.03

[steam]
pressure_threshold = 3000
temperature = 160000
refill_flow = 600
steam_temperature_window = 1300

[steam.preheat_pid_settings]
p = 0.09
d = 0.3
i = 0.02
d_relax = 0.06
i_relax = 2.0

[steam.active_pid_settings]
p = 0.3
d = 0.6
i = 0.03
d_relax = 0.01
i_relax = 0.6
)";

#endif // FULL_CONFIG_HPP

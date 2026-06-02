# @OPENPRESSOD_CONFIG_NAME@(5)

## Name

**@OPENPRESSOD_CONFIG_NAME@** — configuration file for the `openpressod` espresso machine control daemon

## Description

`openpressod` is configured through a single TOML file located at:

```text
@OPENPRESSOD_CONFIG_PATH@
```

The daemon reads this file at startup. If a required key is missing, malformed, or inconsistent with the target hardware, startup fails and the daemon logs a critical error.

The configuration controls hardware binding, safety limits, communication endpoints, logging, watchdog behavior, and brew/steam control parameters.

## File format

The configuration file is written in [TOML](https://toml.io/en/).

## Units

Physical quantities are stored as integers where practical to avoid floating-point drift.

| Dimension | Configured unit | Example |
|---|---|---|
| Temperature | Millidegrees Celsius | `95.0°C` → `95000` |
| Pressure | Millibars | `9.0 bar` → `9000` |
| Flow rate | Milligrams per second | `2.5 g/s` → `2500` |
| Weight | Milligrams | `36 g` → `36000` |
| Time / duration | TOML duration literals | `10ms`, `750ms`, `1s` |

## `[global]`

Main system-wide parameters.

`mains_frequency`
: Integer, required. AC mains frequency in Hz. Allowed values: `50` or `60`.

`mains_zc_pin`
: Integer, required. GPIO pin used for AC mains zero-cross detection.

`user_settings_path`
: String, optional. Absolute path to the runtime user settings file. Default: `/var/lib/openpressod/user_settings.json`.

`brew_profile_path`
: String, optional. Absolute path to the active brew profile file. Default: `/var/lib/openpressod/brew_profile.json`.

`i2c_bus`
: String, optional. I2C bus device path. Default: `/dev/i2c-0`.

`gpio_chip`
: String, optional. GPIO chip device path. Default: `/dev/gpiochip0`.

`pid_state_monitoring_enabled`
: Boolean, optional. Enables detailed diagnostic PID state publishing. Default: `false`.

## `[service]`

Communication interface parameters.

`socket_path`
: String, optional. Unix domain socket path for the gRPC server. Default: `/run/openpresso/openpresso.sock`.

`tcp_enable`
: Boolean, optional. Enables the gRPC server TCP listener. Default: `false`.

`tcp_bind_address`
: String, optional. TCP bind address in `address:port` form. Default: `0.0.0.0:5001`.

## `[log]`

Daemon logging settings.

`console_log_pattern`
: String, optional. Formatting pattern for console logs. Default: `[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v`.

`console_log_level`
: String, optional. Minimum log level. Valid values: `trace`, `debug`, `info`, `warn`, `err`, `critical`, `off`. Default: `info`.

## `[watchdog]`

Hardware watchdog configuration.

`enabled`
: Boolean, optional. Enables the hardware watchdog feed loop. Default: `true`.

`device_path`
: String, optional. Watchdog device path. Default: `/dev/watchdog0`.

`timeout`
: Duration, optional. Watchdog heartbeat timeout. Default: `1s`.

`temperature_limit`
: Integer, optional. Maximum safe boiler temperature in millidegrees Celsius. Default: `170000`.

## `[buttons]`

Physical input buttons.

`power_pin`
: Integer, required. GPIO pin for the machine power button.

`brew_pin`
: Integer, required. GPIO pin for the brew switch.

`steam_pin`
: Integer, required. GPIO pin for the steam switch.

`inverted`
: Boolean, optional. Uses active-low button logic when `true`. Default: `true`.

`power_bistable`
: Boolean, optional. Power switch is latching when `true`. Default: `false`.

`brew_bistable`
: Boolean, optional. Brew switch is latching when `true`. Default: `true`.

`steam_bistable`
: Boolean, optional. Steam switch is latching when `true`. Default: `true`.

`pull_mode`
: String, optional. GPIO pull configuration. Valid values: `PullUp`, `PullDown`, `None`. Default: `PullUp`.

`debounce_period`
: Duration, optional. Button debounce delay. Default: `10ms`.

## `[leds]`

Indicator LEDs.

`power_pin`
: Integer, required. GPIO pin for the power indicator LED.

`brew_pin`
: Integer, required. GPIO pin for the brew indicator LED.

`steam_pin`
: Integer, required. GPIO pin for the steam indicator LED.

`inverted`
: Boolean, optional. Uses active-low output logic when `true`. Default: `false`.

## `[pressure_sensor]`

Water pressure sensor module.

`signal_pin`
: Integer, required. GPIO interrupt or signal pin.

`i2c_addr`
: Integer, optional. I2C address of the ADC. Default: `0x48`.

## `[weight_sensor]`

Scale load cell configuration.

`scale`
: Integer, required. Raw calibration factor used to convert sensor readings to grams.

`signal_pin`
: Integer, required. GPIO interrupt or signal pin.

`filter_timing`
: Duration, optional. Low-pass filtering period. Default: `600ms`.

`i2c_addr`
: Integer, optional. I2C address of the scale ADC. Default: `0x2a`.

## `[temperature_sensor]`

Boiler temperature probe sensor.

`signal_pin`
: Integer, required. GPIO interrupt or signal pin.

`filter_timing`
: Duration, optional. Low-pass filtering period. Default: `750ms`.

`spi_addr`
: String, optional. SPI device node path. Default: `/dev/spidev0.0`.

## `[pump]`

High-pressure water pump control.

`control_pin`
: Integer, required. GPIO pin connected to the pump relay or trigger.

`flow_controller_multiplier`
: Float, required. Proportional scaling coefficient for water flow regulation.

`volume_per_pulse`
: Integer, required. Water volume delivered per pump pulse, in micrograms.

`stall_pressure`
: Integer, required. Pressure limit above which the pump is de-energized for protection, in millibars.

## `[valve]`

Water routing control.

`control_pin`
: Integer, required. GPIO pin connected to the three-way solenoid valve relay.

## `[heater]`

Boiler heating element control.

`control_pin`
: Integer, required. GPIO pin connected to the boiler heating element SSR.

## `[brew]`

Brew mode control parameters.

`temperature`
: Integer, optional. Target extraction temperature in millidegrees Celsius. Default: `95000`.

`pressure`
: Integer, optional. Target extraction pressure in millibars. Default: `9000`.

`brew_temperature_window`
: Integer, optional. Allowed temperature deviation around the target, in millidegrees Celsius. Default: `500`.

### `[brew.pid_settings]`

PID parameters for temperature control during brewing.

`p`
: Float, required. Proportional gain.

`d`
: Float, required. Derivative gain.

`d_relax`
: Float, required. Derivative smoothing coefficient.

`i`
: Float, required. Integral gain.

`i_relax`
: Float, required. Integral anti-windup coefficient.

`f`
: Float, required. Feed-forward coefficient.

`w`
: Float, required. Water-flow compensation coefficient.

`w_decay`
: Float, required. Water-flow compensation decay rate.

## `[steam]`

Steam mode control parameters.

`pressure_threshold`
: Integer, optional. Maximum allowed pressure during steam operations, in millibars. Default: `2500`.

`temperature`
: Integer, optional. Target steaming temperature in millidegrees Celsius. Default: `155000`.

`refill_flow`
: Integer, optional. Boiler refill flow rate during steam operation, in milligrams per second. Default: `500`.

`steam_temperature_window`
: Integer, optional. Allowed temperature deviation around the steam target, in millidegrees Celsius. Default: `1000`.

### `[steam.preheat_pid_settings]`

PID parameters for steam preheating.

`p`
: Float, required. Proportional gain.

`d`
: Float, required. Derivative gain.

`d_relax`
: Float, required. Derivative smoothing coefficient.

`i`
: Float, required. Integral gain.

`i_relax`
: Float, required. Integral anti-windup coefficient.

### `[steam.active_pid_settings]`

PID parameters for active steam control.

`p`
: Float, required. Proportional gain.

`d`
: Float, required. Derivative gain.

`d_relax`
: Float, required. Derivative smoothing coefficient.

`i`
: Float, required. Integral gain.

`i_relax`
: Float, required. Integral anti-windup coefficient.

## Examples

```toml
[global]
mains_frequency = 50
mains_zc_pin = 17

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
control_pin = 27
flow_controller_multiplier = 0.00005
volume_per_pulse = 205000
stall_pressure = 18500

[valve]
control_pin = 22

[heater]
control_pin = 23

[brew.pid_settings]
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
```

## See also

`openpressod(8)`, `openpresso.proto`, `systemd.service(5)`, `sysexits.h(3)`.
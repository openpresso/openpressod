# Configuration {#config}

`openpressod` reads all runtime settings from a single TOML configuration file:

```
@OPENPRESSOD_CONFIG_PATH@
```

The file is read once at startup. If a required key is missing, malformed, or inconsistent with the target hardware, the daemon logs a critical error and exits. There is no runtime reload; restart the service after editing the file.

An example configuration is installed at `@OPENPRESSOD_CONFIG_DIR@/openpressod.conf.example`.

---

## File format

The configuration file uses [TOML](https://toml.io/en/) syntax. Sections are denoted with `[section]` headers; nested tables use dotted notation such as `[brew.pid_settings]`.

---

## Units

Physical quantities are stored as integers where practical to avoid floating-point drift.

| Dimension | Configured unit | Example |
|-----------|----------------|---------|
| Temperature | Millidegrees Celsius | `95.0 °C` → `95000` |
| Pressure | Millibars | `9.0 bar` → `9000` |
| Flow rate | Milligrams per second | `2.5 g/s` → `2500` |
| Weight | Milligrams | `36 g` → `36000` |
| Time / duration | TOML duration literals | `10ms`, `750ms`, `1s` |

---

## `[global]`

Main system-wide parameters.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `mains_frequency` | Integer | **Yes** | — | AC mains frequency in Hz. Allowed values: `50` or `60`. |
| `mains_zc_pin` | Integer | **Yes** | — | GPIO pin number used for AC mains zero-cross detection. |
| `user_settings_path` | String | No | `/var/lib/openpressod/user_settings.json` | Absolute path to the runtime user settings file. |
| `brew_profile_path` | String | No | `/var/lib/openpressod/brew_profile.json` | Absolute path to the active brew profile file. |
| `i2c_bus` | String | No | `/dev/i2c-0` | I²C bus device path. |
| `gpio_chip` | String | No | `/dev/gpiochip0` | GPIO chip device path. |
| `pid_state_monitoring_enabled` | Boolean | No | `false` | Enables detailed diagnostic PID state publishing over the metrics stream. |

---

## `[service]`

Communication interface parameters.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `socket_path` | String | No | `@OPENPRESSOD_SOCKET_PATH@` | Unix domain socket path for the gRPC server. |
| `tcp_enable` | Boolean | No | `false` | Enables the gRPC server TCP listener. See the security note below. |
| `tcp_bind_address` | String | No | `0.0.0.0:5001` | TCP bind address in `address:port` form. Only used when `tcp_enable = true`. |

> **Security:** The daemon provides no built-in authentication or transport encryption. When TCP is enabled, restrict access through firewall rules, interface binding, or network isolation. See [Usage — Security considerations](@ref usage) for details.

---

## `[log]`

Daemon logging settings. Logs are written to the systemd journal (`StandardOutput=journal`).

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `console_log_pattern` | String | No | `[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v` | spdlog formatting pattern for console/journal output. |
| `console_log_level` | String | No | `info` | Minimum log level. Valid values: `trace`, `debug`, `info`, `warn`, `err`, `critical`, `off`. |

---

## `[watchdog]`

Hardware watchdog configuration. The daemon feeds the watchdog device on a heartbeat timer. If the daemon crashes or hangs, the watchdog resets the device, returning hardware outputs to the safe state defined by board-level pin pull configuration.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `enabled` | Boolean | No | `true` | Enables the hardware watchdog feed loop. |
| `device_path` | String | No | `/dev/watchdog0` | Watchdog character device path. |
| `timeout` | Duration | No | `1s` | Watchdog heartbeat interval. |
| `temperature_limit` | Integer | No | `170000` | Maximum safe boiler temperature in millidegrees Celsius. The watchdog stops feeding if this limit is exceeded. |

---

## `[buttons]`

Physical input button configuration. All three button pins are required.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `power_pin` | Integer | **Yes** | — | GPIO pin for the machine power button. |
| `brew_pin` | Integer | **Yes** | — | GPIO pin for the brew switch. |
| `steam_pin` | Integer | **Yes** | — | GPIO pin for the steam switch. |
| `inverted` | Boolean | No | `true` | Uses active-low button logic when `true`. |
| `power_bistable` | Boolean | No | `false` | Power switch is latching (bistable) when `true`. |
| `brew_bistable` | Boolean | No | `true` | Brew switch is latching (bistable) when `true`. |
| `steam_bistable` | Boolean | No | `true` | Steam switch is latching (bistable) when `true`. |
| `pull_mode` | String | No | `PullUp` | GPIO pull configuration. Valid values: `PullUp`, `PullDown`, `None`. |
| `debounce_period` | Duration | No | `10ms` | Button debounce delay. |

---

## `[leds]`

Indicator LED configuration. All three LED pins are required.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `power_pin` | Integer | **Yes** | — | GPIO pin for the power indicator LED. |
| `brew_pin` | Integer | **Yes** | — | GPIO pin for the brew indicator LED. |
| `steam_pin` | Integer | **Yes** | — | GPIO pin for the steam indicator LED. |
| `inverted` | Boolean | No | `false` | Uses active-low output logic when `true`. |

---

## `[pressure_sensor]`

Water pressure sensor module connected over I²C.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `signal_pin` | Integer | **Yes** | — | GPIO interrupt or data-ready signal pin. |
| `i2c_addr` | Integer | No | `0x48` | I²C address of the ADC. |

---

## `[weight_sensor]`

Scale load cell configuration connected over I²C.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `scale` | Integer | **Yes** | — | Raw calibration factor used to convert sensor readings to milligrams. |
| `signal_pin` | Integer | **Yes** | — | GPIO interrupt or data-ready signal pin. |
| `filter_timing` | Duration | No | `600ms` | Low-pass filter time constant. |
| `i2c_addr` | Integer | No | `0x2a` | I²C address of the scale ADC. |

---

## `[temperature_sensor]`

Boiler temperature probe connected over SPI.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `signal_pin` | Integer | **Yes** | — | GPIO interrupt or chip-select signal pin. |
| `filter_timing` | Duration | No | `750ms` | Low-pass filter time constant. |
| `spi_addr` | String | No | `/dev/spidev0.0` | SPI device node path. |

---

## `[pump]`

High-pressure water pump control.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `control_pin` | Integer | **Yes** | — | GPIO pin connected to the pump relay or trigger. |
| `flow_controller_multiplier` | Float | **Yes** | — | Proportional scaling coefficient for water flow regulation. |
| `volume_per_pulse` | Integer | **Yes** | — | Water volume delivered per pump pulse, in micrograms. |
| `stall_pressure` | Integer | **Yes** | — | The theoretical pressure limit at which the pump stalls and produces zero flow, in millibars. |

---

## `[valve]`

Three-way solenoid valve control.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `control_pin` | Integer | **Yes** | — | GPIO pin connected to the solenoid valve relay. |

---

## `[heater]`

Boiler heating element control via a solid-state relay (SSR).

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `control_pin` | Integer | **Yes** | — | GPIO pin connected to the boiler SSR. |

---

## `[brew]`

Brew mode control parameters.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `temperature` | Integer | No | `95000` | Default extraction temperature in millidegrees Celsius if user preset not found. |
| `pressure` | Integer | No | `9000` | Default extraction pressure in millibars if user preset not found. |
| `brew_temperature_window` | Integer | No | `500` | Allowed temperature deviation around the target before brew is gated, in millidegrees Celsius. |

### `[brew.pid_settings]`

PID controller parameters for boiler temperature control during brewing. All fields are required.

| Key | Type | Description |
|-----|------|-------------|
| `p` | Float | Proportional gain. |
| `i` | Float | Integral gain. |
| `i_relax` | Float | Integral anti-windup coefficient. |
| `d` | Float | Derivative gain. |
| `d_relax` | Float | Derivative smoothing coefficient. |
| `f` | Float | Feed-forward coefficient. |
| `w` | Float | Water-flow compensation coefficient. |
| `w_decay` | Float | Water-flow compensation decay rate. |

---

## `[steam]`

Steam mode control parameters.

| Key | Type | Required | Default | Description |
|-----|------|----------|---------|-------------|
| `temperature` | Integer | No | `155000` | Default steaming temperature in millidegrees Celsius if user preset not found. |
| `pressure_threshold` | Integer | No | `2500` | Steam pressure threshold in millibars; boiler refill is disabled above this pressure (should be slightly above maximum possible pressure with opened steam valve). |
| `refill_flow` | Integer | No | `500` | Boiler refill flow rate during steam operation, in milligrams per second. |
| `steam_temperature_window` | Integer | No | `1000` | Temperature deviation window in millidegrees Celsius around the steam target for the steam indicator LED to signal readiness. |

### `[steam.preheat_pid_settings]`

PID parameters for the steam preheat phase. All fields are required.

| Key | Type | Description |
|-----|------|-------------|
| `p` | Float | Proportional gain. |
| `i` | Float | Integral gain. |
| `i_relax` | Float | Integral anti-windup coefficient. |
| `d` | Float | Derivative gain. |
| `d_relax` | Float | Derivative smoothing coefficient. |

### `[steam.active_pid_settings]`

PID parameters for active steam control. All fields are required.

| Key | Type | Description |
|-----|------|-------------|
| `p` | Float | Proportional gain. |
| `i` | Float | Integral gain. |
| `i_relax` | Float | Integral anti-windup coefficient. |
| `d` | Float | Derivative gain. |
| `d_relax` | Float | Derivative smoothing coefficient. |

---

## Complete example

The following is a minimal working configuration covering all required fields. Optional keys use their defaults when omitted.

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

---

## See also

- `openpressod(8)` man page — daemon operation, hardware access, signals, exit codes.
- `openpressod.conf(5)` man page — installed alongside the package.
- [openpresso-proto](https://github.com/openpresso/openpresso-proto) — Protocol Buffers API definitions.

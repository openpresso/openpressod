# Installation {#installation}

This page covers how to install `openpressod` from the Openpresso package repository on a Debian-based system.

---

## Requirements

- A Debian-based Linux distribution (Debian, Ubuntu, Raspberry Pi OS, or a compatible derivative).
- `systemd` as the init system.
- `curl` available for repository setup.

### Supported hardware

`openpressod` is designed for **Gaggia Classic Pro** and similar single-boiler espresso machines upgraded with a compatible controller board. It configures `libopenpresso` for the following hardware components:

| Component | Part |
|-----------|------|
| Weight sensor | NAU7802 (I²C) |
| Temperature sensor | MAX31856 thermocouple amplifier (SPI) |
| Pressure sensor | ADS1115 ADC (I²C) + 12 bar pressure transducer |
| Pump | Vibratory pump driven by a TRIAC-based AC dimmer with zero-cross detection |
| Group valve | 3-way solenoid valve controlled by a relay or TRIAC |
| Boiler heater | Single boiler heating element controlled by a TRIAC-based AC dimmer or SSR |
| Buttons | 3 momentary or latching switches: power, brew, steam (GPIO) |
| LEDs | 3 indicator LEDs: power, brew, steam (GPIO) |

The daemon will not start if the required hardware fields are not set in the configuration file. See [Configuration](@ref config) for pin assignments and hardware parameters.

---

## Adding the package repository

The Openpresso packages are hosted on Cloudsmith. Run the following command to add the repository and its signing key:

```sh
curl -1sLf \
  'https://dl.cloudsmith.io/public/openpresso/@OPENPRESSOD_REPO_CHANNEL@/setup.deb.sh' \
  | sudo -E bash
```

This script registers the repository in `/etc/apt/sources.list.d/` and imports the GPG signing key required to verify packages.

---

## Installing the package

After adding the repository, install `openpressod` with:

```sh
sudo apt-get update
sudo apt-get install openpressod
```

The post-install script runs automatically and performs the following steps:

- Creates the `openpressod` system user and group (no login shell, no home directory creation).
- Creates the persistent data directory (`@OPENPRESSOD_DATA_DIR@`) with restricted permissions.
- Creates the configuration directory (`@OPENPRESSOD_CONFIG_DIR@`).
- Creates the hardware groups `gpio`, `i2c`, `spi`, and `watchdog` if they do not already exist, and adds the `openpressod` user to each.
- Reloads udev rules and re-triggers relevant device subsystems so that group permissions take effect immediately.
- Runs `systemctl daemon-reload` to register the service unit.

The service is **not** started or enabled automatically. Manual configuration is required first.

---

## Post-install configuration

After installation, complete the following steps before starting the service.

**1. Enable I²C and SPI**

`openpressod` requires I²C and SPI to be enabled in the kernel or bootloader configuration. On Raspberry Pi OS or similar boards, use `raspi-config`:

```sh
sudo raspi-config
```

Navigate to **Interface Options** and enable both **I2C** and **SPI**. Reboot for the changes to take effect. On other boards, consult the board-specific documentation for enabling these buses (device tree overlays, kernel module loading, etc.).

**2. Disable the systemd hardware watchdog**

`openpressod` takes exclusive ownership of the hardware watchdog device. systemd also attempts to use the hardware watchdog by default, which conflicts with the daemon. Disable it by editing `/etc/systemd/system.conf` (or a drop-in under `/etc/systemd/system.conf.d/`) and setting:

```ini
[Manager]
RuntimeWatchdogSec=0
RebootWatchdogSec=0
```

Then reload systemd:

```sh
sudo systemctl daemon-reload
```

**3. Verify hardware device nodes**

Confirm that the expected device nodes are present under `/dev` before configuring the daemon. The exact names depend on the target board and enabled overlays:

```sh
# GPIO chip
ls /dev/gpiochip*

# I²C buses
ls /dev/i2c-*

# SPI devices
ls /dev/spidev*

# Hardware watchdog
ls /dev/watchdog*
```

GPIO, I²C, and SPI must be present. The hardware watchdog is optional but **highly recommended** for safety — if the daemon crashes, the watchdog resets the device and returns hardware outputs to a safe state. If any expected device node is missing, check that the corresponding kernel modules are loaded and the relevant device tree overlays are active.

**4. Copy the example configuration:**

```sh
sudo cp @OPENPRESSOD_CONFIG_DIR@/openpressod.conf.example @OPENPRESSOD_CONFIG_PATH@
```

**5. Edit the configuration file:**

```sh
sudo nano @OPENPRESSOD_CONFIG_PATH@
```

Set the hardware-specific required fields for your board — at minimum the GPIO pin assignments for buttons, LEDs, the heater, pump, valve, and the zero-cross detection pin.

If the device nodes found in step 3 differ from the defaults assumed by the configuration, update the corresponding keys in `@OPENPRESSOD_CONFIG_NAME@`:

| Config key | Default value | Section |
|------------|--------------|---------|
| `gpio_chip` | `/dev/gpiochip0` | `[global]` |
| `i2c_bus` | `/dev/i2c-0` | `[global]` |
| `spi_addr` | `/dev/spidev0.0` | `[temperature_sensor]` |
| `device_path` | `/dev/watchdog0` | `[watchdog]` |

See [Configuration](@ref config) for the full reference.

**6. Enable and start the service:**

```sh
sudo systemctl enable --now openpressod
```

**7. Follow the logs to verify startup:**

```sh
journalctl -u openpressod -f
```

> **Note:** The systemd unit includes `ConditionPathExists=@OPENPRESSOD_CONFIG_PATH@`. The service will not start if the configuration file is absent.

---

## Verifying the installation

Check the service status:

```sh
systemctl status openpressod
```

A successful startup looks like:

```
● openpressod.service - OpenPresso Daemon
     Loaded: loaded (/lib/systemd/system/openpressod.service; enabled; ...)
     Active: active (running) since ...
```

If the daemon fails to start, inspect the logs for configuration errors:

```sh
journalctl -u openpressod --no-pager
```

---

## Removing the package

To remove the serive use `apt` (serive will be automatically stoped and disabled before removal):

```sh
sudo apt-get remove openpressod
```


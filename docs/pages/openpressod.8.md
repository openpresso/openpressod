# openpressod(8)

## Name

**openpressod** — Openpresso espresso machine hardware control daemon

## Synopsis

```text
openpressod [-h|--help] [-v|--version] [-c|--conf <path>]
```

`openpressod` is intended to be started and supervised by `systemd`. When run directly, it stays in the foreground and does not daemonize itself.

## Description

`openpressod` is the hardware control daemon for Openpresso-based espresso machines. It is the single authoritative service for direct control of machine hardware: boiler, pump, valves, flow meter, scales, pressure sensors, group temperature sensing, thermoblock-related components, water level sensing, and other supported peripherals attached to the controller.

The primary intended client is a web UI backend process, but advanced users or integrators may also communicate with it directly through compatible gRPC clients. Another gRPC cleint to control the service is the command line tool `openpresso-ctl`. The service can handle multiple concurrent client connections.

## Operation

`openpressod` reads all runtime settings from a single configuration file. By default, the configuration path is:

```text
@OPENPRESSOD_CONFIG_PATH@
```

This path can be overridden using command-line options or by setting the `OPENPRESSOD_CONFIG_PATH` environment variable.

## Options

The following command-line options are supported:

* **-h**, **--help**
  Print help message and exit.

* **-v**, **--version**
  Print version information and exit.

* **-c**, **--conf** `<path>`
  Path to the daemon configuration file. Can also be set via the **OPENPRESSOD_CONFIG_PATH** environment variable. If not specified, the default configuration path is used.

## Environment Variables

* **OPENPRESSOD_CONFIG_PATH**
  Specifies the path to the daemon configuration file. Overridden by the **-c**/**--conf** command-line option if specified.

By default, local IPC uses a Unix domain socket at:

```text
@OPENPRESSOD_SOCKET_PATH@
```

The default persistent data directory is:

```text
@OPENPRESSOD_DATA_DIR@
```

The socket path and data path may be overridden in the configuration file. A TCP listener may also be enabled in the configuration, with an explicitly configured bind interface and port.

## Protocol

`openpressod` exposes its control and monitoring interface over **gRPC**. The canonical Protocol Buffers definitions are maintained in the OpenPresso protocol repository:

- https://github.com/openpresso/openpresso-proto

The public API surface is defined by the protobuf definitions in that repository. The main service definition lives in `openpresso.proto`.

## Hardware access and udev rules


The daemon requires access to GPIO chips, I2C buses, and SPI devices and hardware watchdog under `/dev`. Typical examples include:

- `/dev/gpiochipN`
- `/dev/i2c-N`
- `/dev/spidevX.Y`
- `/dev/watchdogN`

The exact device numbers depend on the target board. The udev rules ensure that the relevant character devices are group-accessible (`gpio`, `i2c`, `spi`, `watchdog`) so that the daemon can be granted access via group membership.

The watchdog device is assigned to the `watchdog` group. The hardware watchdog should remain under daemon control rather than being claimed by `systemd`, because the same watchdog device is generally not intended to be actively owned by multiple watchdog feeders at once.

## Files

| Path | Purpose |
|------|---------|
| `@OPENPRESSOD_CONFIG_PATH@` | Main configuration file. |
| `@OPENPRESSOD_SOCKET_PATH@` | Default Unix domain socket endpoint. |
| `@OPENPRESSOD_DATA_DIR@` | Default persistent data directory for brew profiles and user settings. |
| `/lib/systemd/system/openpressod.service` | `systemd` service unit used to supervise the daemon. |
| `/dev/gpiochip*` | GPIO character devices required for hardware control. |
| `/dev/i2c-*` | I2C bus devices required for sensors or peripheral controllers. |
| `/dev/spidev*` | SPI devices required by supported peripherals. |
| `60-openpressod.rules` | udev rules installed by the package to set device permissions. |

## Configuration

After isntllation config example will be created under `@OPENPRESSOD_CONFIG_DIR@/openpressod.conf.example`. For complete configuration info see `openpressod.conf(5)` page which documents all keys and defaults in detail.

## Security

By default, the safest deployment model is to expose only the Unix domain socket to a trusted local backend and keep TCP disabled unless there is a clear operational need. Since the daemon itself provides no authentication or transport security, any enabled TCP listener must be considered trusted only to the extent that the surrounding network and host controls are trusted.

If TCP is enabled, access control should be enforced externally, for example through interface binding, firewall policy, container or namespace isolation, VPN boundaries, or a separate authenticated gateway. This is especially important because the service controls physical hardware.

## Watchdog and safety

`openpressod` uses a hardware watchdog as part of its safety model. If the daemon crashes, the hardware watchdog is expected to reset the device. After reset, hardware outputs are expected to return to a safe state through board-level default pin pull configuration supplied by the user or platform integrator.

## Users and permissions

The recommended runtime account is:

```text
User=openpressd
Group=openpressod
```

The service installation and udev rules ensure that the data directory, socket directory, and required device nodes are accessible to that account. Access to GPIO, I2C, SPI, and watchdog devices is managed via the `gpio`, `i2c`, `spi`, and `watchdog` groups, as installed by `60-openpressod.rules`.

## Signals

`openpressod` currently does not define a public signal-based control interface. In particular, it does not support configuration reload through `SIGHUP` and should be restarted after configuration changes.

## Exit status

`openpressod` returns the following exit status values:

```text
0   Clean shutdown.
70  Internal software error or uncategorized fatal exception.
```

## Examples

Start the service with `systemd`:

```sh
sudo systemctl start openpressod
```

Enable the service at boot:

```sh
sudo systemctl enable openpressod
```

Inspect service status:

```sh
systemctl status openpressod
```

Follow logs:

```sh
journalctl -u openpressod -f
```

Review installed unit file:

```sh
systemctl cat openpressod
```

Use a gRPC client generated from the protobuf definitions in the OpenPresso protocol repository to connect either to the configured Unix socket or to the configured TCP endpoint.

## Caveats

The daemon provides no built-in authentication, authorization, or transport encryption for its gRPC interface. Any nonlocal exposure therefore depends entirely on deployment controls.

The daemon also relies on platform-specific hardware default states after reset. Incorrect board wiring, pull configuration, or boot-time pin muxing may undermine the intended fail-safe behavior.

## See also

`daemon(7)`, `journalctl(1)`, `man-pages(7)`, `systemd(1)`, `systemd.service(5)`, `systemd.unit(5)`, `openpressod.conf(5)`
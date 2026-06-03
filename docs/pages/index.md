# openpressod {#mainpage}

`openpressod` is the hardware control daemon for Openpresso-based espresso machines. It is built on top of **libopenpresso** and primarily serves to configure and bind libopenpresso components for specific hardware setup, manage brew and steam settings, and expose a unified control and monitoring interface to clients over **gRPC**.

The daemon takes control over all the machine hardware: boiler, pump, valve and LEDs outputs as well as temperature, pressure and weight sensors. It targets the **Gaggia Classic Pro** and similar single-boiler machines modified with compatible controller board.

`openpressod` also serves as a **reference implementation** of a `libopenpresso`-based service. The community is encouraged to use it as a starting point for adopting the Openpresso platform on other coffee machine hardware architectures with different configurations.

---

## Architecture

```
  ┌─────────────────────────────────────────────┐
  │  Clients                                    │
  │  Web UI backend · openpresso-ctl · custom   │
  └──────────────────┬──────────────────────────┘
                     │ gRPC
                     │ Unix domain socket (default)
                     │ TCP (optional)
  ┌──────────────────▼──────────────────────────┐
  │  openpressod daemon                         │
  │  Events dispatching                         │
  │  Profile & settings management              │
  └──────────────────┬──────────────────────────┘
                     │ 
  ┌──────────────────▼──────────────────────────┐
  │  libopenpresso                              │
  │  Run loops, drivers interaction             │
  └─────────────────────────────────────────────┘
                     |
  ┌──────────────────▼──────────────────────────┐
  │  Hardware                                   │
  │  GPIO · I²C · SPI · watchdog                │
  └─────────────────────────────────────────────┘
```

The public API surface is defined by the Protocol Buffers definitions in the
[openpresso-proto](https://github.com/openpresso/openpresso-proto) repository.
The main service definition lives in `openpresso.proto`.

---

## Key characteristics

- **Built on libopenpresso** — hardware abstraction, sensor drivers, and low-level control loops are provided by the underlying library; `openpressod` configures and binds those components to a concrete machine setup.
- **TOML configuration** — all runtime settings (hardware pin assignments, PID tuning, communication endpoints, watchdog parameters) are read from a single configuration file at startup. See [Configuration](@ref config) for the full reference.
- **gRPC interface** — clients connect over a Unix domain socket (`@OPENPRESSOD_SOCKET_PATH@` by default) or an optional TCP listener.
- **Multiple concurrent clients** — the service handles multiple simultaneous gRPC connections.
- **systemd-supervised** — the daemon stays in the foreground and relies on systemd for lifecycle management, logging, and sandboxing.
- **Hardware watchdog** — a hardware watchdog feed loop is used as part of the safety model. If the daemon crashes the watchdog resets the device, returning hardware outputs to a safe state through board-level pin defaults.

---

## Documentation pages

| Page | Contents |
|------|----------|
| [Installation](@ref installation) | Package repository setup, `apt` install, post-install configuration steps. |
| [Usage](@ref usage) | Service management, `openpresso-ctl` command reference, live monitoring, direct gRPC access. |
| [Configuration](@ref config) | Full reference for every key in `@OPENPRESSOD_CONFIG_NAME@`. |

---

## See also

- [openpresso-proto](https://github.com/openpresso/openpresso-proto) — Protocol Buffers API definitions
- [libopenpresso](https://github.com/openpresso/libopenpresso) — underlying libopenpresso repository

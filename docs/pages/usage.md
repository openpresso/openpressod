# Usage {#usage}

This page covers day-to-day operation of `openpressod`.

---

## Service management

`openpressod` is managed by systemd. The daemon stays in the foreground and does not daemonize itself; systemd handles lifecycle, logging, and restart policy.

**Start the service:**

```sh
sudo systemctl start openpressod
```

**Stop the service:**

```sh
sudo systemctl stop openpressod
```

**Enable automatic startup at boot:**

```sh
sudo systemctl enable openpressod
```

**Enable and start in one command:**

```sh
sudo systemctl enable --now openpressod
```

**Check service status:**

```sh
systemctl status openpressod
```

**Follow live logs:**

```sh
journalctl -u openpressod -f
```

**Inspect the installed unit file:**

```sh
systemctl cat openpressod
```

> **Configuration reload:** `openpressod` does **not** support `SIGHUP`-based configuration reload. After editing `@OPENPRESSOD_CONFIG_PATH@`, restart the service for changes to take effect:
>
> ```sh
> sudo systemctl restart openpressod
> ```

---

## openpresso-ctl

`openpresso-ctl` is the companion command-line client for `openpressod`. It can be used to control and monitor the daemon from the terminal — querying machine state, managing brew profiles, adjusting user settings, and streaming live sensor data.

It connects to the daemon over the Unix domain socket (or TCP) using the gRPC interface. Refer to the `openpresso-ctl` documentation for the full command reference.

---

## Direct gRPC access

Any gRPC client generated from the protobuf definitions in [`openpresso-proto`](https://github.com/openpresso/openpresso-proto) can connect to the daemon directly.

Connect to the Unix domain socket:

```
unix://@OPENPRESSOD_SOCKET_PATH@
```

If TCP is enabled in the configuration, connect to the configured address and port (default `0.0.0.0:5001`). The daemon handles multiple concurrent client connections.

---

## Security considerations

By default the daemon exposes only the Unix domain socket, accessible to processes running as the `openpressod` user or group. This is the recommended deployment model.

If TCP is enabled (`tcp_enable = true` in `@OPENPRESSOD_CONFIG_NAME@`), be aware that:

- The daemon provides **no built-in authentication, authorization, or transport encryption**.
- Any enabled TCP listener must be considered trusted only to the extent that the surrounding network and host controls are trusted.
- Access control must be enforced externally through interface binding, firewall rules, container or namespace isolation, VPN boundaries, or a separate authenticated gateway.

This is especially important because the service controls physical hardware.

See [Configuration](@ref config) for the relevant `[service]` keys.

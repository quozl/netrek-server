# Netrek Server Security & Hardening Guide

This document summarizes recommended deployment, configuration, and verification practices for operating `netrek-server` on the public internet.

## 1. Run Unprivileged with systemd Sandboxing

Never run `netrekd`, `ntserv`, `daemon`, or robots as `root`. Reference unit files are provided under `contrib/systemd/`:

- `contrib/systemd/netrek-server.sysusers` — creates the unprivileged `netrek` system account (`/usr/sbin/nologin`).
- `contrib/systemd/netrek-server.tmpfiles` — provisions `var/`, `var/players/`, and `var/ip/` with `0750` ownership for `netrek:netrek`.
- `contrib/systemd/netrek-server.service` — runs `bin/netrekd start` (`Type=forking`) under `NoNewPrivileges=yes`, `ProtectSystem=strict`, `ProtectHome=yes`, empty `CapabilityBoundingSet`, `MemoryDenyWriteExecute=yes`, `UMask=0027`, and bounded file/task limits.

### SysV Shared Memory & `PrivateIPC`
The daemon, `ntserv`, and robots share SysV shared memory and semaphores (`PKEY`). If you do not run external operator CLI tools outside the service unit to inspect/manipulate shared memory, uncomment `PrivateIPC=yes` in `netrek-server.service` for complete IPC namespace isolation.

## 2. Admin Controls (`ADMIN_EXEC` & `ADMIN_PASSWORD`)

In `.sysdef`:
- Keep `ADMIN_EXEC=0` (default) unless shell execution by authenticated admins is explicitly required.
- Set `ADMIN_PASSWORD` to a strong passphrase of at least 8 characters. Do **not** leave it unset or set to `"password"`.
- At startup (`readsysdefaults()` / `validate_admin_config()`), if `ADMIN_EXEC` is non-zero while `ADMIN_PASSWORD` is empty or `"password"`, the server logs a security warning and forces `ADMIN_EXEC` off (`0`).

## 3. Network Exposure & IP Access Control

- **Firewalling:** Expose only the configured client TCP/UDP game ports (by default TCP `2592` and observer port `2593`) and restrict any administrative/GOD ports (`etc/ports`) to trusted local or VPN interfaces.
- **IP filtering:** Manage denied/hidden/duplicate-allowed addresses via `etc/ip/deny`, `etc/ip/hide`, `etc/ip/duplicates`, `etc/ip/whitelist`, and `etc/ip/mute` (supporting exact IPs and `<ip>-<prefix>` CIDR files).
- **Secrets & config permissions:** Keep `etc/` and `var/` readable only by the `netrek` service user (`0750` / `0640`) so `.sysdef` admin passwords and player databases are not world-readable.

## 4. Build Hardening & Verification

Default `./configure` builds enable `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, `-Wl,-z,relro,-z,now` (where supported), and `-Werror` on critical C safety warnings.

Before deploying changes:
```sh
./configure && make && make check
```

To run under AddressSanitizer and UndefinedBehaviorSanitizer with the unit suite and fuzz corpus:
```sh
./configure --enable-sanitizers
make
make check
make -C tests/fuzz check-corpus
```

To stress-test multi-process SysV shared-memory invariants on Linux (requires root for private namespaces):
```sh
sudo tests/shm-stress --duration 30
```

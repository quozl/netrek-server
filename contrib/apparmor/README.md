# AppArmor Profile for `netrek-server`

The profile in [`contrib/apparmor/netrek-server`](netrek-server) provides Mandatory Access Control (MAC) confinement for the Netrek listener (`netrekd` / `lib/newstartd`) and all server components it spawns (`ntserv`, `daemon`, `puck`, `mars`, `robotII`, `inl`, `newbie`, `basep`, `pret`, and helper tools).

---

## 1. Architecture & Design Choices

- **Canonical attachment path (`@{NETREK_DIR}/{lib/newstartd,bin/netrekd}`):**
  `make install` installs `lib/newstartd` and symlinks `bin/netrekd -> lib/newstartd`. Listing both ensures AppArmor attaches cleanly whether invoked via the symlink or resolved target path.
- **Inherited confinement (`ix`):**
  Rules under `@{NETREK_DIR}/bin/** rix` and `@{NETREK_DIR}/lib/** rix` execute child server binaries under the **same** `netrek-server` profile (`ix`), so `ntserv` (which reads untrusted client packets) and `daemon` remain strictly confined and can freely send signals (`signal peer=netrek-server`) and share SysV SHM/semaphores within the profile.
- **Read-only configuration, writable runtime state:**
  - `etc/**` is **read-only** (`r`) by default so a compromised `ntserv` process cannot rewrite `.sysdef`, `ports`, or `banned`/`bypass`.
  - `var/**` allows read, write, and `flock()` (`rwk`) for `netrekd.pid`, logs, player databases, and `var/ip/*`.
- **Shell-escape blocking (no `/bin/sh`):**
  `/bin/sh`, `/bin/dash`, `/bin/bash`, and system utilities outside `@{NETREK_DIR}` are **not** permitted. Even if `ADMIN_EXEC=1` were misconfigured or bypassed, `system()`/`/bin/sh` execution is blocked by the kernel.

---

## 2. Installation & Safe Rollout (Debian / Ubuntu / openSUSE)

### Prerequisites
```sh
sudo apt-get install apparmor apparmor-utils
```

### Step 1: Customize `@{NETREK_DIR}` if not using the default prefix
The profile defaults to:
```apparmor
@{NETREK_DIR}=/usr/local/games/netrek-server
```
If you built with `./configure --prefix=/opt/netrek-server`, update `@{NETREK_DIR}` in `/etc/apparmor.d/netrek-server` accordingly.

### Step 2: Install and validate syntax (dry-run)
```sh
sudo install -m 0644 contrib/apparmor/netrek-server /etc/apparmor.d/netrek-server
sudo apparmor_parser -QTK /etc/apparmor.d/netrek-server
```
(`-Q` skips loading into the kernel, `-T` skips cache, `-K` skips cache writes.)

### Step 3: Start in complain mode and exercise the server
Load the profile in **complain** mode first so AppArmor logs any site-specific paths (`ALLOWED`) without breaking gameplay:
```sh
sudo apparmor_parser -r /etc/apparmor.d/netrek-server
sudo aa-complain /etc/apparmor.d/netrek-server
sudo systemctl restart netrek-server
```
Connect clients, dock, bomb/orbit, run a robot, trigger a save/stats write, then inspect kernel logs:
```sh
sudo journalctl -k -g APPARMOR --since "10 minutes ago"
# or:
sudo aa-logprof
```

### Step 4: Switch to enforce mode
Once clean (or after adding needed site-specific paths to `/etc/apparmor.d/local/netrek-server`):
```sh
sudo aa-enforce /etc/apparmor.d/netrek-server
sudo systemctl restart netrek-server
sudo aa-status | grep netrek-server
```

---

## 3. Site-Local Overrides (`/etc/apparmor.d/local/netrek-server`)

Keep local customizations in `/etc/apparmor.d/local/netrek-server` (automatically included if present) rather than modifying `/etc/apparmor.d/netrek-server`:

### Dynamic IP ban/duplicate updates (`DENY_DUPLICATES=1` or in-game `admin ban` / `admin duplicates`)
```apparmor
@{NETREK_DIR}/etc/ip/deny/** rwk,
@{NETREK_DIR}/etc/ip/duplicates/** rwk,
```

### Legacy shell-based `tools/admin/*` scripts or `SCRIPT_TOURN_START` / `SCRIPT_TOURN_END`
Only add these if you explicitly run shell admin tools or tournament hook scripts (prefer disabling `ADMIN_EXEC=0` and keeping shell access disabled whenever possible):
```apparmor
/bin/sh rix,
/bin/dash rix,
```

### Custom syslog / mail registration (`REGISTERED_USERS`)
If `tools/register` invokes a local MTA or helper, check `aa-logprof` in complain mode and grant the narrowest specific binary rule (`PUx` or `ix`) in `local/netrek-server`.

---

## 4. Layering with `systemd` Sandboxing

AppArmor stacks cleanly with [`contrib/systemd/netrek-server.service`](../systemd/netrek-server.service):
- `systemd` enforces namespace/capability/filesystem/syscall-filter boundaries (`ProtectSystem=strict`, empty `CapabilityBoundingSet=`, `MemoryDenyWriteExecute=yes`, `SystemCallFilter=@system-service`).
- AppArmor enforces path-level MAC policy (`etc/` read-only, `var/` read-write, `ix` binary confinement, no `/bin/sh`).

You may also optionally pin the profile directly inside `[Service]` in `netrek-server.service` (or a drop-in `/etc/systemd/system/netrek-server.service.d/apparmor.conf`) so systemd refuses to start the unit unconfined:
```ini
[Service]
AppArmorProfile=netrek-server
```

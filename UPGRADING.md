# Upgrading a Netrek server

How to replace an older netrek-server release with this one and keep
your players.  It was written after upgrading a 2.17-series i386 server
to 2.20.0 on amd64, and covers what that upgrade found.

The approach is to build the new release beside the old one, test it on
copies of your data, then cut over and keep the old install until you
are sure.  Read the NEWS entry for every release you skip; the 2.20.0
entry lists security fixes and changes an administrator may notice.

In the commands below, `$OLD` is the old install prefix (for example
`/usr/local/games/netrek-server-vanilla`) and `$NEW` is the new one.

## 1. Find out what you are running

**Version.**  The installed `ntserv` contains its version string:

```
strings -a $OLD/lib/ntserv | grep -x -E '2\.[0-9]+'
```

**Where it was built.**  The programs are built with debug
information by default, and it records the build directory and
compiler:

```
readelf --debug-dump=info $OLD/lib/ntserv | grep -m1 DW_AT_comp_dir
readelf --debug-dump=info $OLD/lib/ntserv | grep -m1 DW_AT_producer
```

**The source tree may have moved on.**  Updating the tree without
rebuilding leaves programs that no longer match the source.  Check the
tree's built programs against the installed ones with `cmp`, and the
version string against `include/version.h`.  If they differ, trust the
programs, not the tree.

**Compile-time options.**  These are in the build tree's
`include/config.h`; `config.log` has the `configure` command.
- **Options that change the player record:** LTD_STATS, LTD_PER_RACE
  and GENO_COUNT.  The new build must use the same ones, or it will
  misread `var/players`.
- **Options that change play:** for example AS_CLOAK and
  ROBOTS_STAY_IF_PLAYERS_LEAVE.  Carry them over too, unless you want
  the change.

**Record size check.**  The size of `var/players` must be a whole
number of records:

| LTD_STATS | LTD_PER_RACE | GENO_COUNT | bytes per record |
|---|---|---|---|
| on | off | off | 2,732 |
| on | off | on | 2,736 |

LTD_PER_RACE makes records several times larger.

**Libraries.**  Run `ldd $OLD/lib/ntserv`.  A library marked "not
found" means the old programs, including tools such as `setgame`, only
run with `LD_LIBRARY_PATH` pointing at a saved copy.  An old
`libgdbm.so.3` is a typical case.

**What starts and restarts the server.**  Look for boot scripts, cron
jobs, and any script that probes a port and runs `netrekd` when it
gets no answer.  They must be switched at the cutover (step 5).

## 2. Build the new release beside the old

Don't install over the old release.  Keep it, so you can go back.

**Defaults.**  The 2.20.0 defaults have LTD_STATS on, and
LTD_PER_RACE and GENO_COUNT off.

**Carry your options.**  Edit `include/config.h.in` before running
`configure`.  For example, change `#undef GENO_COUNT` to
`#define GENO_COUNT`.  Commit the edits on a branch, so the build can
be repeated.

**Shared memory key.**  Change it if two servers may run on the same
host at once, for example while testing, or alongside an INL server.
- The default is 128, and `configure --with-inl` uses 129.
- Set another with `#define PKEY 130` in `include/config.h.in`.  The
  tools then use the same key.
- A `PKEY` environment variable also works, but must then be set for
  every program, including cron jobs.

**Architecture.**  Player records are packed, so i386 and amd64 builds
with the same options read and write each other's `players` and
`global` files.

**Build and install.**

```
./autogen.sh
./configure --prefix=$NEW
make
make install
```

`make install` creates each `etc` file from the samples only if it is
absent.  That includes `etc/og/og` with `hm 9`.

**RSA client checking.**  It is built only if a `res-rsa/` directory
is present and the gmp library is found.

## 3. Carry over etc

To see which files you customised, compare each with the old release's
`docs/sample_*`.

| File | What to do |
|---|---|
| `sysdef` | Copy.  Its keywords are unchanged from 2.17 to 2.20.0.  Keywords a build does not know are ignored, so compare with `docs/sample_sysdef` and remove obsolete ones.  `BLOGGING=1` is only useful with a web server for the blog. |
| `ports` | Copy.  Program paths in it are relative to `$NEW/lib`. |
| `metaservers` | Copy. |
| `motd`, `comment` | Copy. |
| `features` | Start from the new sample and re-apply your changes, so you get client sections added since your release. |
| `og/og` | Copy.  It sets robot difficulty; without it the new install has `hm 9`.  Since 2.20.0, newbie and base practice also read it here, not in `lib/og`. |
| `banned`, `bypass`, `nocount`, `clue-bypass`, `ip/*` | Copy. |
| `reserved` | RSA client keys; needed only if RSA is built in. |
| `blosxom.conf`, `ltd-stats-url-prefix` | Only if you run the blog or a statistics web site.  `blosxom.conf` names directories under the prefix. |
| `time`, `tourn.map`, `motd_*` logos | The new defaults, unless you changed them. |

## 4. The player database

The database is `var/players` and `var/global`.  `var/players.index` is
a gdbm cache that the server rebuilds as players log in, so don't copy
it.  It isn't portable between gdbm versions or architectures.

**As it is.**  If the record options match, copy the two files.  This
is the simplest choice, and nothing is changed.

**Dump and load.**  Use the new tools, built with your options:

```
$NEW/lib/tools/scores A > players.dump
$NEW/lib/tools/newscores < players.dump
```

- **Releases before 2.20.0 can't do this.**  Their `scores` and
  `newscores` refuse to run with LTD_STATS.
- **Every player is dumped.**  `scores A` writes one line per player,
  however old.
- **`newscores` replaces both files.**  It writes `var/players` and
  `var/global` from scratch, so run it only into a new install or a
  copy.
- **Mismatched options are refused.**  `newscores` rejects a dump
  whose field count does not match its build, as when LTD_PER_RACE or
  GENO_COUNT differ.
- **Repeatable.**  A second dump after a load is identical to the
  first.
- **What a load changes:**
  - NUL or newline entries in a stored keymap become that key's
    default, because the text format can't carry them;
  - leftover bytes after the end of a name are zeroed;
  - `gameup` in `global` is not kept, but the daemon sets it at start
    anyway.
- **Sharing.**  `scores X` writes the same dump with passwords
  replaced by `X`.

**Tools use their compiled-in paths.**  The old tools act on the old
files and shared memory key, and the new tools on the new ones.  Make
sure you are running the tools you mean.

**Test on copies first.**  Put copies of your files in a scratch
install.  Run a server there inside private network and IPC namespaces
(`unshare -n -i`), so its ports and shared memory can't collide with
the live server.  Then log in, and compare `scores A` dumps.

## 5. Cut over

1. **Switch the restart job first.**  Point whatever restarts the
   server at the new install, so a restart in the middle of the
   cutover starts the new server, not the old.
2. **End the whole game.**
   ```
   $OLD/lib/tools/setgame terminate
   $OLD/lib/tools/setgame wait-for-terminate
   ```
   - `netrekd stop` only stops listening; the daemon, `ntserv`
     processes and robots keep running the old programs.
   - With two servers on one host, each `setgame` reaches only its
     own shared memory key.
   - Both versions' processes have the same names (`netrek-daemon`,
     `ntserv`, `pret`).  Tell them apart by `/proc/PID/exe`, not
     `pkill -f`.
3. **Stop the old listener:** `$OLD/bin/netrekd stop`.
4. **Copy the database.**  Copy `var/players` and `var/global` into
   `$NEW/var`, or load a dump.  Do this only once the old game has
   ended, so nothing is still writing them.
5. **Keep a text backup:** `$NEW/lib/tools/scores A > players-at-cutover.dump`.
6. **Start the new listener:** `$NEW/bin/netrekd start`.
7. **Update anything else that names the old prefix**, such as
   scripts, documentation and your shell's `PATH`.

**Stale pid file.**  `netrekd` refuses to start while
`var/netrekd.pid` names a process it cannot signal.  After a reboot
that is often an unrelated process owned by root.  A restart script
can remove a pid file whose process is not that `netrekd`:

```
clear_stale_pid() {     # pid file, program that should own it
    [ -f "$1" ] || return 0
    [ "$(readlink /proc/$(cat "$1")/exe 2>/dev/null)" = "$2" ] || rm -f "$1"
}
clear_stale_pid $NEW/var/netrekd.pid $NEW/lib/newstartd
```

## 6. Check the result

- **Ports.**  `ss -ltnp` shows the pid of the `netrekd` on each port.
  `readlink /proc/PID/exe` should name `$NEW/lib/newstartd`.
- **Logs.**  Look at `$NEW/var/ERRORS` and `$NEW/var/log`.
- **Metaservers.**  Check they list your server.
- **Log in with an existing account.**
- **Compare dumps.**  Compare a later `scores A` dump with the one
  taken at the cutover.  Outside tournament mode, playing should change
  only that player's last login, and sometimes `deaths.acc`.  The
  server counts that one even outside tournament mode.
- **Rank won't change outside tournament mode.**  Rank rises only by
  promotion, checked when a ship dies, against saved tournament hours
  and ratings.
- **A client can't show a player's saved statistics outside
  tournament mode.**  `SP_LTD` then carries the session's unsaved
  statistics, and `SP_STATS` comes only once the player is in the
  game.  Use `scores A` on the server to check a character.

## 7. Back out if you must

Keep `$OLD` until you are satisfied.  To go back:
1. **Switch the restart job back.**
2. **End the new game:** `$NEW/lib/tools/setgame terminate`.
3. **Stop the new listener:** `$NEW/bin/netrekd stop`.
4. **Keep new accounts, if you want to.**  Copy `$NEW/var/players` and
   `global` back to `$OLD/var`; the layout is the same.  Then remove
   `$OLD/var/players.index`.
5. **Clear a stale `$OLD/var/netrekd.pid`**, then start the old
   listener.

## 8. Afterwards

- **Logs.**  Rotate `var/log` and `var/ERRORS`.  `var/log` gets a line
  for every connection, and neither file is rotated by the server.
- **Backups.**  Back up `var/players` and `var/global` regularly; a
  `scores A` dump is a convenient form.
- **Remove the old install** once you no longer need it.

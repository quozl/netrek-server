# Netrek Shared Memory Stress — Test Results

## Summary

`tests/shm-stress` stresses the shared memory segment used by the daemon
and the ntserv processes, and counts the races that show, so that the
effect of fixes can be measured.

The server was written for one CPU.  On more than one CPU the daemon and
the ntserv processes run at the same time, and nothing serialises their
accesses to the segment.

Result on an unmodified server: in five minutes, 8 of 51,594 player
messages were lost before reaching the daemon, and torpedoes were seen
moving before they were initialised.  No other race showed at natural
rates.  With delays inserted in the race windows, the torpedo count,
torpedo initialisation, phaser and message uplink checks counted races,
which shows that those checks work.

Results after each fix follow, under "Fixes".

## Background

The daemon (`ntserv/daemon.c`) runs the game at 50 updates per second.
In each update, `move()` moves ships, torpedoes and phasers, then signals
each ntserv with `SIGALRM` (`signal_servers()`), and does not wait.  It
then goes on to sight, planet fire, beaming and planets.

Each ntserv (`ntserv/input.c`) builds and sends an update to its client
when signalled, and also handles client packets the moment they arrive,
writing its own player, torpedoes and phaser, and sometimes other
players' torpedoes.

No lock covers the game state.  The semaphores in `ntserv/openmem.c`
cover only choosing a slot, `setgame`, adding a player record and the
connection queue.  Nothing uses atomic operations or memory barriers.
On one CPU, processes interleave only when one is preempted, and a
single instruction read-modify-write such as `andl $~PFWAR, 12(%rax)` is
indivisible; on more than one CPU neither holds.

The windows the test looks for:

| Window | Where | Possible effect |
|---|---|---|
| `t_status = TMOVE` set before the torpedo's fields | `ntserv/torp.c` `ntorp()`, `ntserv/plasma.c` | daemon moves or frees a torpedo using the last one's values |
| `p_ntorp` held in a register from the limit check to the increment | `ntorp()`, as compiled by gcc -O2 | daemon's decrement lost; the count stays above the torpedoes in flight, and the ship can fire fewer than eight until it dies |
| `p_ntorp` decremented by `subw $1` without a `lock` prefix | `ntserv/daemon.c` `udtorps()` | ntserv's increment lost; the count falls below the torpedoes in flight, the `p_ntorp == MAXTORP` limit check lets a ninth torpedo through, and it takes the first slot of the next player |
| `t_status == TMOVE` tested, then `TOFF` set | `ntserv/socket.c` `handleDetMReq()` | a torpedo expiring in between is freed twice, and the count drops by two, as for a lost increment |
| `ph_status` and `ph_fuse` set before `ph_target` and `ph_damage` | `ntserv/phaser.c` `phaser()` | daemon applies the previous shot's damage to the previous target |
| uplink index advanced before the message is written | `ntserv/smessage.c` `do_message()` | daemon skips the entry and the message is lost |
| `mc_current` advanced before the message is copied | `do_message_next_daemon()` | an ntserv reads a stale entry |
| `p_flags` stored from a copy loaded earlier | `ntserv/daemon.c` `udplayersight()` | a player's flag change, such as shields, is undone |

## The test

`tests/shm-stress` builds the tracked files of the source tree into a
scratch prefix, and runs the server, a monitor and sixteen clients in
private network, IPC and PID namespaces, so it needs root, and does not
disturb any other server on the host.

Clients, eight on each of two teams:

- **torpers** fire torpedoes and detonate them, either one of each per
  step, so that the daemon frees torpedoes while ntserv fires more, or
  in salvoes, firing every step for a random 2 to 6 seconds, then
  detonating all; ntserv allows one torpedo per update, so a salvo has
  eight live after a second or so, then asks ntserv for more while
  eight are live, and as the first expire, from 4 seconds;
- **attackers** phaser victims in an arena away from planets,
  alternating targets;
- **victims** are phasered;
- all send numbered messages to themselves, and all but victims toggle
  shields.

Clients act at random moments, as players do.  Acting just after each
update arrives would keep clear of the daemon's critical sections, as
the update has a fixed phase within the daemon's cycle.

`tests/shm-stress-monitor.c` attaches to the segment and polls it
continuously, using one CPU.  It checks the invariants below, counts the
activity, and keeps the ships fuelled, cool, healed and in place.

The scratch install has `CLASSICTOURN=1`, and `127.0.0.2`, the clients'
address, in `etc/ip/whitelist` and `etc/ip/duplicates`, so that all
sixteen clients may join and send messages without limit.

## Running

```
tests/shm-stress                       # 120 s against this source tree
tests/shm-stress --duration 300        # longer
tests/shm-stress --widen 1000          # positive control, see below
tests/shm-stress --work DIR --reuse    # build into DIR once, then reuse
```

The build takes a minute or so, joining thirty seconds, then the stress
runs for the duration.  The test needs root, `unshare`, `ip`, git,
Python 3, a C compiler and the autotools, and exits 1 if any race was seen, 0 if none,
and 2 if the test itself failed, such as a client failing to join.

Output is the monitor's events and progress, then a summary:

```
activity: torps sent 68285 seen 17532, detonations sent 68258 seen 17518,
phaser shots sent 1278 hits 1278, messages sent 51594 queued 51586
received 51586, shield commands 7848, deaths 0
races:
  ntorp-drift              0
  ...
```

## Reading the results

| Count | Meaning |
|---|---|
| `ntorp-drift` | a ship's `p_ntorp` differed from the number of live torpedoes it owns for more than 100 ms |
| `nplasma-drift` | the same for plasma torpedoes |
| `torp-slot` | a live torpedo stayed for more than 20 ms in a slot of another player |
| `torp-uninit` | a torpedo was seen moving before `ntorp()` set its position; this shows the window is open, the daemon moving the torpedo in it is much rarer |
| `phaser-stale` | a phaser hit's target or damage changed after the daemon had applied it |
| `message-lost-uplink` | a message sent by a client never reached the daemon's queue |
| `message-lost-downlink` | a message in the queue never reached its client |
| `message-duplicated` | a message reached its client twice |
| `shield-lost` | a shield command never took effect |
| `shield-undone` | a shield command took effect, then reverted |

Ships that die are excluded from the drift checks, as `enter()` zeroes
`p_ntorp` while the last ship's torpedoes may still be flying.

The monitor polls, so it can miss a short window; counts are lower
bounds.  The phaser window is reached only rarely, as the daemon applies
phaser hits on every fifth update, ten times a second.

## Positive control

`--widen US` inserts a busy delay of US microseconds into each window in
the scratch build only, at these points:

- `phaser()`, after `ph_fuse` is set;
- `ntorp()`, between reading and writing `p_ntorp`, or before changing
  it in one step;
- `udtorps()` in the daemon, between reading and writing `p_ntorp`, or
  before changing it in one step;
- `udplayersight()` in the daemon, between reading and writing
  `p_flags` to clear `PFSEEN`, or before changing it in one step;
- `ntorp()`, while the torpedo is being initialised;
- `handleDetMReq()`, between testing and setting `t_status`, or before
  doing both in one step;
- `do_message()`, after an uplink entry is chosen, before it is
  written, for players only;
- `do_message_requeue()` in the daemon, after a queue entry is chosen,
  before it is written.

The build reports each delay inserted.  When a fix changes the code at a
window, its delay may no longer apply, and the build says so.  Where a
fix moves the window, the test has a delay for the code before and
after the fix, at the same point, so that one version of the test
measures both.

## Results

Run on 5 November 2026, source at commit `4bb3652`, Ubuntu 26.04.1,
kernel 6.8, gcc 15.2, Python 3.14, Intel i7-1255U with 12 CPUs.

| | Unmodified, 300 s | `--widen 1000`, 60 s |
|---|---|---|
| torpedoes seen | 17,532 | 4,220 |
| phaser hits | 1,278 | 333 |
| messages sent | 51,594 | 13,187 |
| shield commands | 7,848 | 2,070 |
| `ntorp-drift` | 0 | 50 |
| `nplasma-drift` | 0 | 0 |
| `torp-slot` | 0 | 0 |
| `torp-uninit` | 32 | 310 |
| `phaser-stale` | 0 | 1 |
| `message-lost-uplink` | **8** | 638 |
| `message-lost-downlink` | 0 | 0 |
| `message-duplicated` | 0 | 0 |
| `shield-lost`, `shield-undone` | 0 | 0 |

With 1 ms delays, about 5% of messages were lost, as expected for a 1 ms
window in a 20 ms update, and phaser hits, applied every 100 ms, about
1% were expected to race.

Not shown by these runs: torpedoes in another player's slot.  The
torpers of that version of the test fired salvoes of eight requests,
but ntserv allows one torpedo per update, so they never had eight live
and never asked for a ninth; and the only delay was in `ntorp()`, which
loses the daemon's decrements, leaving the count too high, never too
low.  With the present salvoes and the delay in `udtorps()`, the same
server source widened by 1000 us for 60 s put torpedoes in another
player's slot 39 times.

Not shown by these runs: loss or duplication of messages on their way
from the daemon's queue to the clients; and lost shield commands.
Neither path had a delay then; the daemon's queue has one since, see
"Message queues" under "Fixes".

Plasma is not fired by any client.

## Fixes

### Torpedo limit

`ntorp()` refused a torpedo only when `p_ntorp == MAXTORP`, and when
none of the ship's own slots was free, it took the slot after its last,
the first of the next player.  It now refuses when `p_ntorp >= MAXTORP`,
or when none of its own slots is free.  Plasma torpedoes and the
sturgeon special weapons are limited the same way.  The count still
drifts, but a drift no longer reaches another player.

Run on 7 November 2026, test at commit `e89cbe8`, same host as above.
"Before" is commit `9b1555a`, with the server source of `4bb3652`.

| | Before, `--widen 1000`, 600 s | After, `--widen 1000`, 600 s | Before, 1200 s | After, 1200 s |
|---|---|---|---|---|
| torpedoes seen | 18,618 | 29,635 | 57,528 | 57,124 |
| phaser hits | 2,463 | 2,464 | 4,827 | 4,831 |
| messages sent | 99,604 | 99,537 | 195,430 | 195,492 |
| deaths | 0 | 1 | 4 | 4 |
| `ntorp-drift` | 6,974 | 12,439 | 0 | 0 |
| `nplasma-drift` | 0 | 0 | 0 | 0 |
| `torp-slot` | **293** | **0** | 0 | 0 |
| `torp-uninit` | 3,450 | 5,708 | 147 | 150 |
| `phaser-stale` | 7 | 12 | 0 | 0 |
| `message-lost-uplink` | 4,709 | 4,887 | 16 | 11 |
| `message-lost-downlink`, `message-duplicated` | 0 | 0 | 0 | 0 |
| `shield-lost`, `shield-undone` | 0 | 0 | 0 | 0 |

With delays, torpedoes ran into another player's slot 293 times before
the fix, and never after.  Without delays, the count did not drift in
twenty minutes, before or after, and the other counts were about the
same.

Fewer torpedoes were seen before the fix with delays; why was not
established.  Ships die now and then with the present salvoes, which
keep torpedoes in flight for longer, before and after the fix alike.

### Message queues

`do_message()` moved a queue's index to a new entry before writing the
entry.  The daemon, reading a player's uplink at that moment, found the
entry not yet valid, skipped it, and the message was lost.  The daemon
did the same as it copied each message into the main queue, so that an
ntserv reading the queue at that moment could send its player the
entry's previous message, and miss the new one.

Each writer now fills in the entry, then moves the index with a release
store, and each reader loads the index with acquire, and reads entries
up to it.  Robots and tools share the out-of-band queue, and now hold a
lock while they add to it.  Readers in robots and tools are not yet
changed.

Run on 7 November 2026, same host as above.  "Before" is commit
`45b5487`.  The 1200 s run after the fix also has commit `c380bd8`,
which stops ntserv changing the flags of shared message entries; the
others do not.

| | Before, `--widen 1000`, 600 s | After, `--widen 1000`, 600 s | Before, 1200 s | After, 1200 s | After, 3000 s |
|---|---|---|---|---|---|
| torpedoes seen | 29,261 | 29,267 | 57,592 | 57,646 | 141,959 |
| phaser hits | 2,446 | 2,453 | 4,828 | 4,835 | 11,918 |
| messages sent | 99,600 | 99,617 | 195,439 | 195,603 | 482,826 |
| messages received | 94,883 | 99,617 | 195,424 | 195,603 | 482,826 |
| deaths | 7 | 0 | 3 | 0 | 10 |
| `ntorp-drift` | 12,223 | 12,042 | 0 | 0 | 0 |
| `torp-slot` | 0 | 0 | 0 | 0 | 0 |
| `torp-uninit` | 5,742 | 5,757 | 256 | 467 | 846 |
| `phaser-stale` | 18 | 16 | 0 | 0 | 0 |
| `message-lost-uplink` | **4,716** | **0** | **15** | **0** | **0** |
| `message-lost-downlink` | 1 | 0 | 0 | 0 | 0 |
| `message-duplicated` | 1 | 0 | 0 | 0 | 0 |
| `shield-lost` | 0 | 0 | 0 | 0 | 1 |
| `shield-undone` | 0 | 0 | 0 | 0 | 0 |

With delays, the uplink lost 4,716 messages before the fix, and none
after.  Without delays, 15 were lost in twenty minutes before, and none
in seventy minutes after.  Earlier runs of the same source before the
fix lost 11, 16 and 27.

The delay in the daemon's copy into the main queue caused only one lost
and one duplicated message before the fix.  Each ntserv reads the queue
when the daemon signals it, which is after the copy, so few reads
overlap the delay, and the result after the fix is weak evidence for
that queue.

`torp-uninit` was higher after the fix, in back to back runs and in the
3000 s run.  The fix does not change `ntorp()`, and why was not
established.  One shield command was lost in the 3000 s run, the first
in any run; the client saw no change in its flags after sending it.

### Weapon status

`ntorp()` set a torpedo's status to `TMOVE` before its other fields, so
that the daemon could move, hit with, or free a torpedo with the
previous torpedo's position, fuse and owner.  Plasma torpedoes, the
sturgeon special weapons and phasers did the same; a phaser could hit
the previous shot's target with the previous shot's damage, and its
fuse could be left below zero, so that it never freed again.

Each now fills in its fields, then sets the status with a release
store, and the daemon reads the status with an acquire load.  Where the
daemon frees a torpedo or a phaser, it reads what it needs first, then
sets the status free with a release store, as the owner may take the
slot again at once.  A sturgeon nuke that fails to detonate is no longer
detonated anyway.

Readers of torpedo and phaser status in ntserv's updates to clients, in
robots and in tools are not yet changed.

Run on 8 November 2026, same host as above.  "Before" is commit
`42e2013`.

| | Before, `--widen 1000`, 600 s | After, `--widen 1000`, 600 s | After, 1200 s | Before, 1200 s |
|---|---|---|---|---|
| torpedoes seen | 29,585 | 29,463 | 57,089 | 57,804 |
| phaser hits | 2,454 | 2,455 | 4,828 | 4,827 |
| messages sent | 99,632 | 99,443 | 195,516 | 195,440 |
| messages received | 99,632 | 99,443 | 195,516 | 195,440 |
| deaths | 5 | 1 | 1 | 7 |
| `ntorp-drift` | 11,923 | 7,254 | 0 | 0 |
| `torp-slot` | 0 | 0 | 0 | 0 |
| `torp-uninit` | **5,702** | **0** | **0** | **332** |
| `phaser-stale` | **15** | **0** | 0 | 0 |
| `message-lost-uplink` | 0 | 0 | 0 | 0 |
| `message-lost-downlink`, `message-duplicated` | 0 | 0 | 0 | 0 |
| `shield-lost` | 1 | 0 | 0 | 0 |
| `shield-undone` | 0 | 0 | 0 | 0 |

With delays, torpedoes were seen moving before they were initialised
5,702 times before the fix, and phaser hits changed after the daemon
applied them 15 times; after the fix, neither.  Without delays, 332
torpedoes were seen before they were initialised in twenty minutes
before the fix, and none after, in a run that counted no race at all.

`ntorp-drift` with delays was lower after the fix; this was not
investigated.  A second shield command was lost, before the fix, with
delays.  Both lost shield commands were by attackers.  The daemon also
changes ships' flags, and this fix does not touch that path.

### Torpedo counts

ntserv incremented `p_ntorp` and `p_nplasmatorp`, and the daemon
decremented them, each by reading, changing and writing the count, so
that one could undo the other.  `handleDetMReq()` and `detothers()`
tested that a torpedo was moving, then detonated it, and a phaser that
destroyed a plasma did the same; when the torpedo expired in between,
the daemon freed it twice and decremented the count twice.

The counts are now changed in one indivisible step, and the test and
the detonation are one compare and swap.  `enter()` still sets the
counts to zero while the last ship's torpedoes may be in flight, and a
plasma deflected in ping-pong mode may still lose a count if the daemon
frees it during the deflection.  Robots and tools are not yet changed.

Run on 9 November 2026, same host as above.  "Before" is commit
`7145d71`.

| | Before, `--widen 1000`, 600 s | After, `--widen 1000`, 600 s | After, 1200 s | Before, 1200 s |
|---|---|---|---|---|
| torpedoes seen | 29,724 | 29,695 | 57,787 | 57,860 |
| phaser hits | 2,454 | 2,445 | 4,830 | 4,830 |
| messages sent | 99,519 | 99,537 | 195,699 | 195,662 |
| messages received | 99,519 | 99,537 | 195,699 | 195,662 |
| deaths | 0 | 7 | 0 | 1 |
| `ntorp-drift` | **7,273** | **0** | **0** | **1** |
| all other counts | 0 | 0 | 0 | 0 |

With delays, `p_ntorp` drifted 7,273 times before the fix, and never
after.  Without delays, it drifted once in twenty minutes before the
fix, to 0 with one torpedo live, a count too low, which before the
torpedo limit fix would let a ninth torpedo into the next player's
slots.  Both runs after the fix counted no race at all.

## Lessons for testing

- Clients that act just after each update never overlap the daemon's
  critical sections.  The first version of this test saw no races at
  all, even with 1 ms delays, for that reason.
- New ships start with shields up.
- Before tournament mode, a new player may not join a team of four.
  With `CLASSICTOURN=1`, or in tournament mode, a new player may not
  join a team more than one player ahead.  The test sets
  `CLASSICTOURN=1` and alternates the clients' teams.
- `DUPLICATES=3` limits connections from one address, unless the address
  is in `etc/ip/duplicates`.
- Unwhitelisted players may send one message every three seconds.
- Clients from `127.0.0.1` count as robots under `IS_ROBOT_BY_HOST`.
- Early versions of this test lost several percent of messages between
  the daemon's queue and the clients.  The loss stopped as the clients
  were changed to parse their stream faster, to wait for their ship
  before acting, and to join without errors; which change stopped it
  was not established.  A rise in `message-lost-downlink` may be the
  clients' doing.

## References

- `tests/shm-stress` — driver, clients and fault injection
- `tests/shm-stress-monitor.c` — monitor
- `ntserv/daemon.c` — `move()`, `signal_servers()`, `udtorps()`,
  `udphaser()`, `udplayersight()`
- `ntserv/input.c` — ntserv input loop
- `ntserv/torp.c`, `ntserv/plasma.c`, `ntserv/phaser.c` — weapons
- `ntserv/socket.c` — `handleDetMReq()`
- `ntserv/smessage.c` — message uplink and queue
- `ntserv/openmem.c` — shared memory and semaphores
- `tools/test_client_protocol.py` — packet sizes used by the clients

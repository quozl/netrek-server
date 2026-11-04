# Netrek Client-to-Server Protocol — Test Results

## Summary

The harness in `tools/test_client_protocol.py` drives the TCP game protocol
(port 2592) all the way from connection to **orbiting a planet**, then sends
protocol-violating payloads against an ASAN-instrumented server.

Result: the valid flow reaches orbit reliably, and the server handled every
malformed payload with no AddressSanitizer report.

> This document supersedes an earlier version that reported the flow stopping
> at "6/8 steps" with SP_PICKOK described as an unexplained server-config
> issue, and that reported "ASAN CLEAN" based on runs whose packet parser was
> desynchronised. Those conclusions were wrong; see **Corrections** below.

## Environment

Server built with AddressSanitizer and installed properly:

```bash
CFLAGS="-g -O0 -fsanitize=address -fno-omit-frame-pointer" \
LDFLAGS="-fsanitize=address" ./configure
make -C ntserv
make install                          # -> /usr/local/games/netrek-server

cd /usr/local/games/netrek-server/lib
./newstartd </dev/null >/tmp/ns.out 2>&1 &   # self-daemonises, listens on 2592
```

`make install` is required: it creates `etc/time`, `var/players`, `etc/ports`,
`etc/sysdef` and the rest of the runtime tree. Without them the daemon never
sets `GU_GAMEOK`, `tournamentMask()` returns 0 (no team allowed), and every
`CP_OUTFIT` is refused with `SP_PICKOK(state=0)`. The installed `ntserv` is
dynamically linked against `libasan.so.8`; ntserv redirects its stderr to
`var/ERRORS`, so any ASAN report lands there.

## Valid protocol test — PASS

```
[1] TCP connect                     ✓ connected 127.0.0.1:2592
[2] CP_SOCKET + initial burst       ✓ slot 0 assigned; 40 SP_PLANET_LOC learned
[3] CP_LOGIN                        ✓ SP_LOGIN accept=1
[4] CP_OUTFIT team=FED ship=CRUISER ✓ SP_PICKOK state=1 (alive)
[5] locate nearest planet           ✓ Earth (#0) at (20000,80000)
[6] fly + orbit                     ✓ PFORBIT set (flags 0x00809)
[7] CP_BYE
```

Reproduced on every run. The harness parses the entire server stream with zero
leftover bytes and zero desync, tracks its own ship position from `SP_PLAYER`,
finds the nearest planet from `SP_PLANET_LOC`, steers with
`CP_DIRECTION`/`CP_SPEED` using the same bearing formula as the reference
client (`atan2(dx, -dy)/pi*128`), and on reaching < `ENTORBDIST` (900u) at
speed ≤ `ORBSPEED` (2) sends `CP_ORBIT`, then confirms the `PFORBIT` (0x0008)
flag in the server's flag updates.

## Malformed payload test — server robust, no ASAN findings

Each case opens its own connection, completes the `CP_SOCKET` handshake, sends
one violation, then the harness reconnects cleanly to confirm the server is
still serving. `var/ERRORS` is checked for ASAN output afterwards.

| Case | Bytes | Server result |
|------|------:|---------------|
| Oversized `CP_LOGIN` payload | 4100 | healthy — reads fixed login struct; trailing bytes parsed as packets, first unknown type trips "unknown packet type, aborting" and the connection is dropped |
| Unhandled packet type 200 | 4 | healthy — "unknown packet type: -56, aborting" |
| Short `CP_OUTFIT` (2 of 4 bytes) | 2 | healthy |
| Out-of-range team/ship | 4 | healthy — `getEntry()` rejects |
| Negative `CP_SPEED` | 4 | healthy |
| `CP_DIRECTION` flood (2000 pkts) | 8000 | healthy |

No `AddressSanitizer` signature appeared in `var/ERRORS` for any case
(`grep -c AddressSanitizer var/ERRORS` → 0). The server's packet loop already
rejects unrecognised type codes and aborts that one connection, which is what
neutralises the oversized/garbage inputs.

## Corrections to the previous results

Two independent defects in the earlier harness — not the server — were why it
never reached play, and both are fixed:

1. **Wrong packet sizes → stream desync.** The size table had `SP_YOU=28`
   (correct 32), `SP_LOGIN=100` (correct 104), `SP_PLAYER=14` (correct 12), and
   defaulted every unlisted type to 4 bytes. One wrong length shifts every
   subsequent read; the harness then interpreted bytes from the middle of
   packets as type codes (hence the "packet type 0" and ASCII-value "types" in
   the old logs) and never recognised `SP_PICKOK`. The corrected table is
   verified against `ntserv/genspkt.c sizes[]`, the `py-struct` comments in
   `include/packets.h`, and `gytha` via `struct.calcsize()`.

2. **Wrong ship pick.** It sent ship type 0 while labelling it "ASSAULT". Type
   0 is actually SCOUT, which `ntserv/getentry.c` gates behind
   `sc_minimal_offense`; ASSAULT is type 4 and also gated. A fresh guest (offense
   0, lowest rank) can only be sure of **CRUISER (type 2)**, the one ship with
   no requirement. The harness now sends CRUISER.

The `ship`/`team` constants in the harness were also mislabelled
(`ASSAULT=0`, `FED/ROM/KLI/ORI = 0..3` as if the team field were the bitmask);
they now match `include/defs.h` (`SCOUT=0…STARBASE=5`) with a note that the
`CP_OUTFIT` team field is the index 0–3 while SP packets carry the bitmask
`FED=1,ROM=2,KLI=4,ORI=8`.

## Mechanically observed hardening opportunities (backward compatible)

These come from what the tests actually exercised; none change the existing v4
wire format:

- **A. Softer recovery from unknown packets.** Today an unknown type aborts the
  whole connection. Skipping one packet's worth of bytes for a *known-but-
  unsupported* type (and only aborting on a truly unrecognised one) would make
  a client that emits a stray packet more resilient. No wire change.

- **B. Opt-in length framing.** Lengths are implied by type, so an old client
  cannot skip a future variable-length packet. A `CP_FEATURE`/`SP_FEATURE`
  (mechanism already present) opt-in "length-prefixed packets" mode would let
  new clients resynchronise; clients that do not request it see no change.

- **C. Incomplete-packet timeout.** A truncated final packet is held pending
  more bytes; a per-connection timeout would bound the read state a silent
  client can pin.

## Reproduce

```bash
# server (installed, ASAN) running as above, then:
python3 tools/test_client_protocol.py                    # join + orbit
python3 tools/test_client_protocol.py --malformed-tests  # + fuzz cases
python3 tools/test_client_protocol.py --trace            # show packet types
grep -c AddressSanitizer /usr/local/games/netrek-server/var/ERRORS
```

## References

- `tools/test_client_protocol.py` — harness (RESULTS block at end of file)
- `gytha/packets.py` — reference client packet formats
- `include/packets.h`, `ntserv/genspkt.c` — packet structs and size table
- `ntserv/getentry.c` — outfit acceptance rules (ship offense/rank gates)
- `ntserv/orbit.c`, `include/defs.h` — orbit distance/speed rules

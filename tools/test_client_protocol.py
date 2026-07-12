#!/usr/bin/env python3
"""
Netrek Client-to-Server Protocol Test Harness

Drives the TCP game protocol used by Netrek clients, from initial connection
all the way to joining the game, choosing a ship, and flying into orbit around
a planet.  It then (optionally) sends protocol-violating payloads to look for
robustness problems under AddressSanitizer.

The protocol is a stream of fixed-size binary packets.  Each packet begins with
a one-byte type code; the length is determined by the type.  CP_* packets are
client->server, SP_* packets are server->client.  All multi-byte integers are
network byte order (big-endian).

Protocol state machine (TCP mode):

  1. TCP connect (default port 2592)
  2. CP_SOCKET   -> announce protocol version
  3. SP_MOTD ... -> message of the day, plus initial galaxy burst
     SP_YOU       -> our player slot number is assigned
  4. CP_LOGIN    -> credentials (name, password, login)
     SP_LOGIN     -> accept flag + keymap
     SP_YOU       -> refreshed player state
  5. CP_OUTFIT   -> choose team and ship type
     SP_PICKOK    -> state==1 means the ship was accepted and we are alive
  6. In play: continuous SP_PLAYER / SP_TORP / SP_PHASER / SP_FLAGS / SP_YOU,
     and we issue CP_DIRECTION / CP_SPEED / CP_ORBIT etc.

Server installation (REQUIRED before this test will reach play):

  The server reads several files that only exist after a proper install.
  Without them the daemon never marks the game "up" (GU_GAMEOK), so no team is
  ever allowed and CP_OUTFIT is refused with SP_PICKOK(state=0).  Notably
  etc/time and var/players must exist.  Install and run with:

    make install                                  # -> /usr/local/games/netrek-server-vanilla
    cd /usr/local/games/netrek-server-vanilla/lib
    ./newstartd </dev/null >/tmp/ns.out 2>&1 &    # self-daemonizes, listens on 2592

  (newstartd forks ntserv per connection; ntserv starts the shared-memory
  daemon on demand.  Do not start the daemon by hand -- it exits when idle.)

Ship choice matters:

  A brand-new player has offense rating 0 and the lowest rank.  In
  ntserv/getentry.c the SCOUT, DESTROYER, ASSAULT, BATTLESHIP, STARBASE and
  GALAXY picks are all gated by minimum offense/rank.  CRUISER is the only ship
  with no requirement, so it is the only pick guaranteed to be accepted for a
  fresh guest login.  (The earlier version of this harness sent ship type 0,
  which is SCOUT -- gated by sc_minimal_offense -- and mislabelled it
  "ASSAULT"; that pick could be refused.)

Packet sizes:

  The SP size table below is the authoritative fixed-size layout a basic
  (non-"short-packet", non-feature) client receives, verified against
  ntserv/genspkt.c `sizes[]`, include/packets.h `py-struct` comments, and the
  reference client gytha/packets.py via struct.calcsize().
  Getting these wrong desynchronises the stream: after one short read the next
  "type" byte is taken from the middle of the previous packet, which is exactly
  the failure the previous revision of this harness exhibited (it reported
  packet "type 0" and ASCII bytes as packet types, and never saw SP_PICKOK).

Malformed-payload testing:

  With ntserv built under -fsanitize=address, run with --malformed-tests.  Each
  case opens its own connection, completes the CP_SOCKET handshake, sends one
  protocol violation, and then we confirm the server is still healthy by making
  a fresh well-formed connection.  Any ASAN report is written by ntserv to
  <server>/var/ERRORS (ntserv redirects its stderr there), so check that file
  after a run.  See the RESULTS section at the bottom of this file.

Reference:
  gytha/packets.py                        - reference client packet formats
  include/packets.h                       - server packet structs + py-struct
  ntserv/socket.c, ntserv/genspkt.c       - server packet I/O and size table
  ntserv/getentry.c, ntserv/orbit.c       - outfit acceptance and orbit rules
"""

import socket
import struct
import time
import sys
import math
import argparse
from typing import Optional, Dict, Tuple
from dataclasses import dataclass, field


# --- Server-to-client packet type codes (include/packets.h) ---------------
SP_MESSAGE, SP_PLAYER_INFO, SP_KILLS, SP_PLAYER = 1, 2, 3, 4
SP_TORP_INFO, SP_TORP, SP_PHASER, SP_PLASMA_INFO, SP_PLASMA = 5, 6, 7, 8, 9
SP_WARNING, SP_MOTD, SP_YOU, SP_QUEUE, SP_STATUS = 10, 11, 12, 13, 14
SP_PLANET, SP_PICKOK, SP_LOGIN, SP_FLAGS, SP_MASK = 15, 16, 17, 18, 19
SP_PSTATUS, SP_BADVERSION, SP_HOSTILE, SP_STATS = 20, 21, 22, 23
SP_PL_LOGIN, SP_RESERVED, SP_PLANET_LOC = 24, 25, 26
SP_UDP_REPLY, SP_SEQUENCE, SP_GENERIC_32 = 28, 29, 32
SP_SHIP_CAP, SP_PING, SP_FEATURE = 39, 46, 60

SP_NAMES = {
    1: "SP_MESSAGE", 2: "SP_PLAYER_INFO", 3: "SP_KILLS", 4: "SP_PLAYER",
    5: "SP_TORP_INFO", 6: "SP_TORP", 7: "SP_PHASER", 8: "SP_PLASMA_INFO",
    9: "SP_PLASMA", 10: "SP_WARNING", 11: "SP_MOTD", 12: "SP_YOU",
    13: "SP_QUEUE", 14: "SP_STATUS", 15: "SP_PLANET", 16: "SP_PICKOK",
    17: "SP_LOGIN", 18: "SP_FLAGS", 19: "SP_MASK", 20: "SP_PSTATUS",
    21: "SP_BADVERSION", 22: "SP_HOSTILE", 23: "SP_STATS", 24: "SP_PL_LOGIN",
    25: "SP_RESERVED", 26: "SP_PLANET_LOC", 28: "SP_UDP_REPLY",
    29: "SP_SEQUENCE", 32: "SP_GENERIC_32", 39: "SP_SHIP_CAP", 46: "SP_PING",
    60: "SP_FEATURE",
}

# Authoritative fixed sizes (bytes, including the type byte) for the packets a
# basic TCP client receives.  See module docstring for how these were verified.
SP_SIZES = {
    SP_MESSAGE: 84, SP_PLAYER_INFO: 4, SP_KILLS: 8, SP_PLAYER: 12,
    SP_TORP_INFO: 8, SP_TORP: 12, SP_PHASER: 16, SP_PLASMA_INFO: 8,
    SP_PLASMA: 12, SP_WARNING: 84, SP_MOTD: 84, SP_YOU: 32, SP_QUEUE: 4,
    SP_STATUS: 28, SP_PLANET: 12, SP_PICKOK: 4, SP_LOGIN: 104, SP_FLAGS: 8,
    SP_MASK: 4, SP_PSTATUS: 4, SP_BADVERSION: 4, SP_HOSTILE: 4, SP_STATS: 56,
    SP_PL_LOGIN: 52, SP_RESERVED: 20, SP_PLANET_LOC: 28, SP_UDP_REPLY: 8,
    SP_SEQUENCE: 4, SP_GENERIC_32: 32, SP_SHIP_CAP: 60, SP_PING: 8,
    SP_FEATURE: 88,
}

# --- Client-to-server packet type codes ------------------------------------
CP_MESSAGE, CP_SPEED, CP_DIRECTION, CP_PHASER = 1, 2, 3, 4
CP_TORP, CP_QUIT, CP_LOGIN, CP_OUTFIT = 6, 7, 8, 9
CP_ORBIT, CP_BOMB, CP_SOCKET, CP_BYE, CP_UPDATES = 14, 17, 27, 29, 31

# --- Game constants (include/defs.h, include/struct.h) ---------------------
SOCKVERSION, UDPVERSION = 4, 10
# Ship types (defs.h): note these are NOT 0=ASSAULT.
SCOUT, DESTROYER, CRUISER, BATTLESHIP, ASSAULT, STARBASE = 0, 1, 2, 3, 4, 5
# CP_OUTFIT team field is an index 0-3 (server computes 1<<team); the team
# BITMASK reported in SP packets is FED=1, ROM=2, KLI=4, ORI=8.
TEAM_FED, TEAM_ROM, TEAM_KLI, TEAM_ORI = 0, 1, 2, 3
# Player status (struct.h)
PFREE, POUTFIT, PALIVE, PEXPLODE, PDEAD = 0, 1, 2, 3, 4
# Player flags (struct.h)
PFSHIELD, PFREPAIR, PFBOMB, PFORBIT, PFCLOAK = 0x0001, 0x0002, 0x0004, 0x0008, 0x0010
# Orbit mechanics (defs.h): within 900 units and speed <= 2.
ENTORBDIST, ORBSPEED = 900, 2


@dataclass
class GameState:
    """Everything the harness learns from the server packet stream."""
    slot: Optional[int] = None
    login_accepted: bool = False
    pickok: Optional[int] = None          # last SP_PICKOK state (0 reject, 1 ok)
    status: Optional[int] = None          # our SP_PSTATUS status
    flags: int = 0                        # our player flags (SP_YOU / SP_FLAGS)
    x: Optional[int] = None               # our galactic x (SP_PLAYER)
    y: Optional[int] = None               # our galactic y
    speed: int = 0                        # our current speed (SP_PLAYER)
    planets: Dict[int, Tuple[int, int, str]] = field(default_factory=dict)
    warnings: list = field(default_factory=list)


class ProtocolError(Exception):
    pass


class NetrekClient:
    """Minimal but correct TCP client for the Netrek game protocol."""

    def __init__(self, timeout: float = 3.0, trace: bool = False):
        self.timeout = timeout
        self.trace = trace
        self.sock: Optional[socket.socket] = None
        self.rxbuf = b""
        self.connected = False
        self.gs = GameState()

    # -- connection ---------------------------------------------------------
    def connect(self, host: str, port: int = 2592) -> bool:
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.sock.settimeout(self.timeout)
            self.sock.connect((host, port))
            self.connected = True
            return True
        except OSError as e:
            print(f"  ✗ connect failed: {e}")
            return False

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
        self.connected = False

    def send(self, data: bytes) -> bool:
        if not self.connected:
            return False
        try:
            self.sock.sendall(data)
            return True
        except OSError as e:
            print(f"  ✗ send failed: {e}")
            self.connected = False
            return False

    # -- receive / parse ----------------------------------------------------
    def pump(self, duration: float = 0.4):
        """Read for up to `duration` seconds, parsing and dispatching every
        complete packet.  Raises ProtocolError on stream desync."""
        deadline = time.time() + duration
        while True:
            remaining = deadline - time.time()
            if remaining <= 0:
                break
            self.sock.settimeout(remaining)
            try:
                chunk = self.sock.recv(8192)
            except socket.timeout:
                break
            except OSError as e:
                self.connected = False
                raise ProtocolError(f"recv failed: {e}")
            if not chunk:
                self.connected = False
                raise ProtocolError("server closed connection")
            self.rxbuf += chunk
            self._drain()

    def _drain(self):
        off = 0
        buf = self.rxbuf
        n = len(buf)
        while off < n:
            ptype = buf[off]
            size = SP_SIZES.get(ptype)
            if size is None:
                # A basic client should never see any other type.  Refusing to
                # guess is deliberate: guessing a size is what desynced the
                # previous harness.
                raise ProtocolError(
                    f"unexpected packet type {ptype} at offset {off} "
                    f"(stream desync or unhandled feature packet)")
            if off + size > n:
                break                      # partial packet; wait for more
            self._handle(ptype, buf[off:off + size])
            off += size
        self.rxbuf = buf[off:]

    def _handle(self, ptype: int, pkt: bytes):
        gs = self.gs
        if ptype == SP_YOU:
            # !bbbbbbxxIlllhhhh : pnum, hostile, swar, armies, tractor, flags...
            f = struct.unpack('!bbbbbbxxIlllhhhh', pkt)
            gs.slot = f[1]
            gs.flags = f[6]
        elif ptype == SP_PLAYER and gs.slot is not None:
            # !bbBbll : pnum, dir, speed, x, y
            _, pnum, _dir, speed, x, y = struct.unpack('!bbBbll', pkt)
            if pnum == gs.slot:
                gs.x, gs.y, gs.speed = x, y, speed
        elif ptype == SP_FLAGS and gs.slot is not None:
            # !bbbxI : pnum, tractor, flags
            _, pnum, _tractor, flags = struct.unpack('!bbbxI', pkt)
            if pnum == gs.slot:
                gs.flags = flags
        elif ptype == SP_PSTATUS and gs.slot is not None:
            _, pnum, status = struct.unpack('!bbbx', pkt)
            if pnum == gs.slot:
                gs.status = status
        elif ptype == SP_PICKOK:
            gs.pickok = struct.unpack('!bbxx', pkt)[1]
        elif ptype == SP_LOGIN:
            gs.login_accepted = bool(struct.unpack('!bbxxl96s', pkt)[1])
        elif ptype == SP_PLANET_LOC:
            # !bbxxll16s : pnum, x, y, name
            _, pnum, x, y, name = struct.unpack('!bbxxll16s', pkt)
            gs.planets[pnum] = (x, y, name.split(b'\0')[0].decode('ascii', 'replace'))
        elif ptype in (SP_WARNING, SP_MESSAGE):
            text = pkt[4:].split(b'\0')[0].decode('ascii', 'replace').strip()
            if text:
                gs.warnings.append(text)
        if self.trace and ptype not in (SP_PLAYER, SP_TORP, SP_PHASER,
                                        SP_PLASMA, SP_TORP_INFO, SP_PLASMA_INFO):
            print(f"      <- {SP_NAMES.get(ptype, ptype)}")

    # -- client packet senders ---------------------------------------------
    def cp_socket(self):
        return self.send(struct.pack('!bbbxI', CP_SOCKET, SOCKVERSION, UDPVERSION, 0))

    def cp_login(self, name: str, password: str = "", login: str = ""):
        login = login or name
        return self.send(struct.pack(
            '!bbxx16s16s16s', CP_LOGIN, 0,
            name.encode('ascii')[:15], password.encode('ascii')[:15],
            login.encode('ascii')[:15]))

    def cp_outfit(self, team: int, ship: int):
        return self.send(struct.pack('!bbbx', CP_OUTFIT, team, ship))

    def cp_direction(self, direction: int):
        return self.send(struct.pack('!bBxx', CP_DIRECTION, direction & 0xff))

    def cp_speed(self, speed: int):
        return self.send(struct.pack('!bbxx', CP_SPEED, speed))

    def cp_orbit(self, state: int):
        return self.send(struct.pack('!bbxx', CP_ORBIT, state))

    def cp_bye(self):
        return self.send(struct.pack('!bxxx', CP_BYE))


def bearing(mx: int, my: int, tx: int, ty: int) -> int:
    """Netrek direction (0-255) from (mx,my) toward (tx,ty).
    Matches the gytha client's gytha/coords.py s2d()."""
    return int(math.atan2(tx - mx, my - ty) / math.pi * 128.0 + 0.5) & 0xff


def dist(ax, ay, bx, by) -> float:
    return math.hypot(ax - bx, ay - by)


def join_and_orbit(host: str, port: int, timeout: float, trace: bool) -> bool:
    """Full valid-protocol test: connect, log in, pick a CRUISER, fly to the
    nearest planet, and orbit it.  Returns True only if we actually orbit."""
    print("\n" + "=" * 70)
    print("VALID PROTOCOL TEST: connect -> login -> outfit -> fly -> orbit")
    print("=" * 70)

    c = NetrekClient(timeout=timeout, trace=trace)

    print("\n[1] TCP connect")
    if not c.connect(host, port):
        return False
    print(f"  ✓ connected to {host}:{port}")

    try:
        print("\n[2] CP_SOCKET handshake + initial burst")
        c.cp_socket()
        c.pump(1.0)
        if c.gs.slot is None:
            print("  ✗ no SP_YOU received (slot not assigned)")
            return False
        print(f"  ✓ slot assigned: {c.gs.slot}; learned {len(c.gs.planets)} planet locations")

        print("\n[3] CP_LOGIN")
        c.cp_login("robot", "", "robot")
        c.pump(1.0)
        if not c.gs.login_accepted:
            print("  ✗ login not accepted (SP_LOGIN accept flag not set)")
            return False
        print("  ✓ login accepted (SP_LOGIN accept=1)")

        print("\n[4] CP_OUTFIT team=FED ship=CRUISER")
        c.gs.pickok = None
        c.cp_outfit(TEAM_FED, CRUISER)
        deadline = time.time() + 5.0
        while time.time() < deadline and c.gs.pickok is None:
            c.pump(0.5)
        if c.gs.pickok != 1:
            print(f"  ✗ outfit refused (SP_PICKOK state={c.gs.pickok})")
            if c.gs.warnings:
                print(f"    server said: {c.gs.warnings[-1]!r}")
            return False
        print("  ✓ SP_PICKOK state=1 -> ship accepted, we are alive")

        print("\n[5] Locate nearest planet")
        # settle a moment so our SP_PLAYER position is current
        c.pump(0.6)
        if c.gs.x is None:
            print("  ✗ never received our own SP_PLAYER position")
            return False
        target = min(c.gs.planets.items(),
                     key=lambda kv: dist(c.gs.x, c.gs.y, kv[1][0], kv[1][1]))
        pnum, (px, py, pname) = target
        d0 = dist(c.gs.x, c.gs.y, px, py)
        print(f"  ✓ at ({c.gs.x},{c.gs.y}); nearest planet #{pnum} "
              f"{pname!r} at ({px},{py}), distance {d0:.0f}")

        print("\n[6] Fly toward planet and orbit")
        orbited = False
        fly_deadline = time.time() + 30.0
        last_report = 0.0
        while time.time() < fly_deadline:
            d = dist(c.gs.x, c.gs.y, px, py)
            # choose speed: brisk far out, crawl close in so we can orbit
            want_speed = 2 if d < 3000 else 6
            c.cp_direction(bearing(c.gs.x, c.gs.y, px, py))
            c.cp_speed(want_speed)
            # once in range and slow enough, request orbit
            if d <= ENTORBDIST and c.gs.speed <= ORBSPEED:
                c.cp_orbit(1)
            c.pump(0.3)
            if time.time() - last_report > 2.0:
                print(f"    dist={d:6.0f} speed={c.gs.speed} "
                      f"flags=0x{c.gs.flags:05x}"
                      f"{' PFORBIT' if c.gs.flags & PFORBIT else ''}")
                last_report = time.time()
            if c.gs.flags & PFORBIT:
                orbited = True
                break

        if orbited:
            print(f"  ✓ ORBITING planet #{pnum} {pname!r} "
                  f"(flags=0x{c.gs.flags:05x}, PFORBIT set)")
        else:
            print(f"  ✗ failed to establish orbit within time limit "
                  f"(last dist={dist(c.gs.x, c.gs.y, px, py):.0f}, "
                  f"speed={c.gs.speed}, flags=0x{c.gs.flags:05x})")

        print("\n[7] CP_BYE")
        c.cp_bye()
        return orbited

    except ProtocolError as e:
        print(f"  ✗ protocol error: {e}")
        return False
    finally:
        c.close()


# --- malformed-payload cases ----------------------------------------------
# Each returns raw bytes to transmit after a normal CP_SOCKET handshake.
def _mal_oversized_login() -> bytes:
    # CP_LOGIN header then far more string data than the 3x16 the server reads.
    return struct.pack('!bbxx', CP_LOGIN, 0) + b'A' * 4096

def _mal_invalid_type() -> bytes:
    return bytes([200, 0, 0, 0])           # type code with no handler

def _mal_short_packet() -> bytes:
    return bytes([CP_OUTFIT, 0])           # 2 bytes; handler expects 4

def _mal_invalid_team() -> bytes:
    return struct.pack('!bbbx', CP_OUTFIT, 99, 99)   # team/ship out of range

def _mal_negative_speed() -> bytes:
    return struct.pack('!bbxx', CP_SPEED, -100)      # speed clamped server-side?

def _mal_flood() -> bytes:
    return b''.join(struct.pack('!bBxx', CP_DIRECTION, d & 0xff)
                    for d in range(0, 2000))         # 8 KB of direction spam

MALFORMED_CASES = [
    ("oversized CP_LOGIN payload (4 KB)", _mal_oversized_login),
    ("unhandled packet type 200", _mal_invalid_type),
    ("short CP_OUTFIT (2 of 4 bytes)", _mal_short_packet),
    ("out-of-range team/ship in CP_OUTFIT", _mal_invalid_team),
    ("negative CP_SPEED", _mal_negative_speed),
    ("CP_DIRECTION flood (2000 packets)", _mal_flood),
]


def server_healthy(host: str, port: int, timeout: float) -> bool:
    """A fresh well-formed handshake must still get us a slot."""
    probe = NetrekClient(timeout=timeout)
    if not probe.connect(host, port):
        return False
    try:
        probe.cp_socket()
        probe.pump(1.0)
        return probe.gs.slot is not None
    except ProtocolError:
        return False
    finally:
        probe.cp_bye()
        probe.close()


def run_malformed_tests(host: str, port: int, timeout: float) -> bool:
    print("\n" + "=" * 70)
    print("MALFORMED PAYLOAD TESTS (run ntserv under -fsanitize=address)")
    print("=" * 70)
    print("After each case we reconnect cleanly to confirm the server still")
    print("serves new clients.  Check <server>/var/ERRORS for ASAN reports.\n")

    all_ok = True
    for name, gen in MALFORMED_CASES:
        c = NetrekClient(timeout=timeout)
        if not c.connect(host, port):
            print(f"[case] {name}: ✗ could not connect")
            all_ok = False
            continue
        try:
            c.cp_socket()
            try:
                c.pump(0.5)
            except ProtocolError:
                pass
            payload = gen()
            c.send(payload)
            # give the server a moment to process (and possibly crash)
            try:
                c.pump(0.4)
            except ProtocolError:
                pass  # server dropping/aborting our connection is fine
        finally:
            c.close()

        healthy = server_healthy(host, port, timeout)
        mark = "✓" if healthy else "✗ SERVER UNHEALTHY"
        print(f"[case] {name}: sent {len(payload)} bytes -> {mark}")
        if not healthy:
            all_ok = False

    print(f"\n  server still healthy after all cases: "
          f"{'yes' if all_ok else 'NO'}")
    return all_ok


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Netrek client-to-server protocol test harness")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=2592)
    ap.add_argument("--timeout", type=float, default=3.0)
    ap.add_argument("--malformed-tests", action="store_true",
                    help="also send protocol-violating payloads (needs ASAN)")
    ap.add_argument("--trace", action="store_true",
                    help="print received packet types")
    args = ap.parse_args()

    print("=" * 70)
    print("NETREK CLIENT-TO-SERVER PROTOCOL TEST HARNESS")
    print("=" * 70)
    print(f"server {args.host}:{args.port}  timeout={args.timeout}s")

    ok = join_and_orbit(args.host, args.port, args.timeout, args.trace)

    if args.malformed_tests:
        ok = run_malformed_tests(args.host, args.port, args.timeout) and ok

    print("\n" + "=" * 70)
    print("SUMMARY:", "all tests passed ✓" if ok else "FAILURES ✗")
    print("=" * 70)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())


# ===========================================================================
# RESULTS
# ===========================================================================
#
# Environment: ntserv + daemon built with
#   CFLAGS="-g -O0 -fsanitize=address -fno-omit-frame-pointer"
#   LDFLAGS="-fsanitize=address"
# and installed with `make install` (so etc/time, var/players, etc/ports and
# the rest of the runtime files exist).  Server launched with:
#   cd /usr/local/games/netrek-server-vanilla/lib && ./newstartd </dev/null &
# ntserv links libasan.so.8; its stderr (hence any ASAN report) is redirected
# to <server>/var/ERRORS.
#
# Valid protocol test (join, pick ship, fly to orbit): PASS.
#   connect -> CP_SOCKET -> slot 0 assigned + 40 SP_PLANET_LOC learned
#   -> CP_LOGIN accepted -> CP_OUTFIT(FED, CRUISER) -> SP_PICKOK state=1 (alive)
#   -> nearest planet is Earth (#0) at (20000,80000)
#   -> steer with CP_DIRECTION/CP_SPEED, and on reaching <900u at speed<=2,
#      CP_ORBIT(1) -> our player flags gain PFORBIT (0x0008).  Reproduced on
#      every run.
#
# Two independent defects in the previous revision of this harness were the
# reason it never reached this point, and both are fixed here:
#   1. Wrong SP packet sizes (SP_YOU 28->32, SP_LOGIN 100->104, SP_PLAYER
#      14->12) and a default of 4 bytes for every unlisted type.  A single
#      wrong size desynchronises the byte stream; the old harness then read
#      "type" bytes out of the middle of packets, printing bogus types 0 and
#      raw ASCII, and never recognising SP_PICKOK.
#   2. It sent ship type 0 (SCOUT, gated by sc_minimal_offense) while calling
#      it "ASSAULT".  A fresh guest has offense 0, so the only ship guaranteed
#      to be accepted is CRUISER (ntserv/getentry.c imposes no requirement on
#      it).  See also that the server refuses outfit entirely until the game is
#      marked up (GU_GAMEOK), which only happens with a proper install -- the
#      original "SP_PICKOK never arrives" symptom was that misconfiguration.
#
# Malformed payload tests (ntserv under ASAN): server remained healthy after
# every case and NO AddressSanitizer report was produced in var/ERRORS.
#   - oversized CP_LOGIN (4 KB): server reads only the fixed login struct; the
#     trailing bytes are interpreted as further packets, the first unknown type
#     trips "unknown packet type: NN, aborting" and the connection is dropped.
#   - unhandled type 200, short CP_OUTFIT, out-of-range team/ship, negative
#     speed, 2000-packet CP_DIRECTION flood: all handled without crash; new
#     connections continue to be served.
#
# So the current server is robust against these inputs.  The mechanically
# observed, backward-compatible protocol-hardening opportunities are:
#
#   A. Unknown/oversized handling is "abort the whole connection".  A client
#      that accidentally emits an unknown type loses its whole session.  A
#      backward-compatible improvement is to skip exactly one packet's worth of
#      bytes when the type is known-but-unsupported, and only abort on a type
#      that is entirely unrecognised.  (No wire change; old clients unaffected.)
#
#   B. There is no explicit negotiated framing/length.  Every length is implied
#      by the type, so a future variable-length packet cannot be skipped by an
#      old client.  Reserving an optional "length follows" feature (negotiated
#      via CP_FEATURE/SP_FEATURE, which already exists) would let clients skip
#      unknown packets instead of desyncing -- opt-in, so existing clients that
#      do not request it see the wire format unchanged.
#
#   C. CP_ packet handlers are dispatched purely by a fixed size table; a short
#      final packet is retained until more bytes arrive, which lets a client
#      that sends a truncated packet then goes silent hold server read state.
#      A per-connection incomplete-packet timeout would bound that.
#
# These are observations for discussion, not code changes; none require
# breaking the existing v4 wire protocol.


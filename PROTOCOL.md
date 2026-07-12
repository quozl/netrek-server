# Netrek Network Protocol

This document describes the network protocol used for communication between Netrek clients and the Netrek server.

## Overview

The Netrek protocol is a binary packet-based protocol that runs over both TCP and UDP. Communication begins with TCP, and UDP can be negotiated later for improved performance. The protocol uses network byte order (big-endian) for all multi-byte values.

## Multicast Server Discovery

Netrek supports automatic server discovery using multicast UDP queries.

### Protocol

Server discovery uses multicast UDP on the address `224.0.0.1` port `3521`. This is an extension to the metaserver query protocol.

#### Query Packet

Clients send a single question mark (`?`) character as a discovery query.

#### Response Packet Format

Servers reply with a comma-separated text packet with the following fields:

- `s` - Literal lowercase "s" marking this as a server reply (vs. metaserver reply)
- `type` - Server type (e.g., "B" for Bronco, "V" for Vanilla)
- `comment` - Text from server's comment file, displayed in metaserver window
- `ports` - Number of ports this server is accepting connections on
- For each port:
  - `port` - Port number to connect to
  - `players` - Number of players currently playing on this port
  - `queue` - Number of players waiting in queue for this port

#### Example

```
s,B,Test Server,1,2592,0,0
```

This indicates a Bronco server with no players and no queue on port 2592.

### Configuration

#### Server Configuration

1. Add the `3521u` line to the ports configuration (sample_ports)
2. Add a `comment` file in SYSCONFDIR describing the server
3. Restart netrekd
4. Test with `metaget localhost` and `metaget 224.0.0.1`

#### Client Configuration

No configuration is necessary. When using the `-m` (metaserver) option or running without a specified server, clients will automatically query metaservers and use multicast discovery.

### Implementation Details

- The netrekd process receives UDP query packets (see newstartd/newstartd.c)
- netrekd forks the players program to handle queries (see tools/players)
- Queries are logged to the server log file

## TCP Connection

The main game protocol runs over TCP, establishing a reliable ordered stream of netrek packets.

### Connection Establishment

After establishing a TCP connection, the following sequence occurs:

1. Client sends `CP_SOCKET` packet identifying protocol version
2. Client optionally sends `CP_FEATURE` to indicate support for feature packets
3. Server sends `SP_MOTD` (Message of the Day) lines
4. Server sends `SP_FEATURE` if client sent `CP_FEATURE`
5. Server sends `SP_QUEUE` packets (zero or more) if queue is present
6. Server sends `SP_YOU` packet with assigned player slot number
7. Player enters login state

### Packet Format

Netrek packets on TCP consist of:
- A 1-byte packet type identifier
- Variable-length data depending on packet type

Multiple packets are laid end-to-end in the TCP stream. Since TCP is stream-based, packet boundaries may not align with socket reads. Clients and servers must maintain a buffer and reassemble fragmented packets.

```
+-----+----------+-----+----------+-----+----------+
| TCP Stream                                      |
+-----+----------+-----+----------+-----+----------+
| pkt | data ... | pkt | data ... | pkt | data ... |
+-----+----------+-----+----------+-----+----------+
```

### Packet Types

#### Version Numbers

- `SOCKVERSION = 4` - TCP protocol version
- `UDPVERSION = 10` - UDP protocol version

#### Server-to-Client Packet Types (SP_)

| ID | Name | Purpose |
|----|------|---------|
| 1 | SP_MESSAGE | Chat message |
| 2 | SP_PLAYER_INFO | General player info |
| 3 | SP_KILLS | Player kill count |
| 4 | SP_PLAYER | Player position and direction |
| 5 | SP_TORP_INFO | Torpedo status |
| 6 | SP_TORP | Torpedo position |
| 7 | SP_PHASER | Phaser status and direction |
| 8 | SP_PLASMA_INFO | Plasma torpedo status |
| 9 | SP_PLASMA | Plasma torpedo position |
| 10 | SP_WARNING | Server warning message |
| 11 | SP_MOTD | Message of the Day line |
| 12 | SP_YOU | Player status (your ship info) |
| 13 | SP_QUEUE | Queue position |
| 14 | SP_STATUS | Galaxy status |
| 15 | SP_PLANET | Planet armies and facilities |
| 16 | SP_PICKOK | Ship outfit accepted |
| 17 | SP_LOGIN | Login response |
| 18 | SP_FLAGS | Player flags |
| 19 | SP_MASK | Tournament mode mask |
| 20 | SP_PSTATUS | Player status |
| 21 | SP_BADVERSION | Invalid version |
| 22 | SP_HOSTILE | Hostility settings |
| 23 | SP_STATS | Player statistics |
| 24 | SP_PL_LOGIN | New player login notification |
| 26 | SP_PLANET_LOC | Planet name and location |
| 28 | SP_UDP_REPLY | UDP status notification |
| 29 | SP_SEQUENCE | Sequence number packet |
| 30 | SP_SC_SEQUENCE | Semi-critical sequence packet |
| 31 | SP_RSA_KEY | RSA key for binary verification |
| 39 | SP_SHIP_CAP | Server ship capabilities |
| 40 | SP_S_REPLY | Reply to send-short request |
| 41 | SP_S_MESSAGE | Variable-length message |
| 42 | SP_S_WARNING | Warning with 4 bytes |
| 43 | SP_S_YOU | Your status (variable) |
| 44 | SP_S_YOU_SS | Your ship status |
| 45 | SP_S_PLAYER | Variable-length player packet |
| 46 | SP_PING | Ping packet |
| 47 | SP_S_TORP | Variable-length torpedo packet |
| 48 | SP_S_TORP_INFO | Torpedo with info |
| 49 | SP_S_8_TORP | Optimized torpedo packet |
| 50 | SP_S_PLANET | Planet info |
| 56 | SP_S_SEQUENCE | Compressed sequence packet |
| 57 | SP_S_PHASER | Phaser packet |
| 58 | SP_S_KILLS | Kill count |
| 59 | SP_S_STATS | Statistics |
| 60 | SP_FEATURE | Feature support |
| 61 | SP_RANK | Rank information |
| 62 | SP_LTD | Limited statistics |

#### Client-to-Server Packet Types (CP_)

| ID | Name | Purpose |
|----|------|---------|
| 1 | CP_MESSAGE | Send message |
| 2 | CP_SPEED | Set ship speed |
| 3 | CP_DIRECTION | Change direction |
| 4 | CP_PHASER | Fire phaser |
| 5 | CP_PLASMA | Fire plasma torpedo |
| 6 | CP_TORP | Fire regular torpedo |
| 7 | CP_QUIT | Self-destruct ship |
| 8 | CP_LOGIN | Login with credentials |
| 9 | CP_OUTFIT | Select team and ship |
| 10 | CP_WAR | Change war status |
| 11 | CP_PRACTR | Request practice robot |
| 12 | CP_SHIELD | Raise/lower shields |
| 13 | CP_REPAIR | Enter/exit repair mode |
| 14 | CP_ORBIT | Orbit planet/starbase |
| 15 | CP_PLANLOCK | Lock on planet |
| 16 | CP_PLAYLOCK | Lock on player |
| 17 | CP_BOMB | Bomb planet |
| 18 | CP_BEAM | Beam armies up/down |
| 19 | CP_CLOAK | Toggle cloaking |
| 20 | CP_DET_TORPS | Detonate enemy torpedoes |
| 21 | CP_DET_MYTORP | Detonate own torpedo |
| 22 | CP_COPILOT | Toggle copilot mode |
| 23 | CP_REFIT | Refit to different ship |
| 24 | CP_TRACTOR | Toggle tractor beam |
| 25 | CP_REPRESS | Toggle pressor beam |
| 26 | CP_COUP | Coup home planet |
| 27 | CP_SOCKET | Identify socket/version |
| 28 | CP_OPTIONS | Save player options |
| 29 | CP_BYE | Disconnect |
| 30 | CP_DOCKPERM | Set docking permissions |
| 31 | CP_UPDATES | Set update rate |
| 32 | CP_RESETSTATS | Reset statistics |
| 34 | CP_SCAN | Request player scan |
| 35 | CP_UDP_REQ | Request UDP mode |
| 36 | CP_SEQUENCE | Sequence number |
| 37 | CP_RSA_KEY | RSA key |
| 38 | CP_PLANET | Cross-check planet info |
| 42 | CP_PING_RESPONSE | Ping response |
| 43 | CP_S_REQ | Short packet request |
| 44 | CP_S_THRS | Threshold setting |
| 45 | CP_S_MESSAGE | Variable-length message |
| 60 | CP_FEATURE | Feature support |

### Packet Structure Details

All packets in memory use network byte order (big-endian) for multi-byte integers. Character arrays are generally not NUL-terminated.

#### Example Structures

**CP_SOCKET** (Client to Server, Type 27)
```c
struct socket_cpacket {
    char type;           // 27
    char version;        // Protocol version (SOCKVERSION)
    char udp_version;    // UDP version support (UDPVERSION)
    char pad3;
    u_int socket;        // Socket identifier for reconnection
};
```

**SP_YOU** (Server to Client, Type 12)
```c
struct you_spacket {
    char type;           // 12
    char pnum;           // Player number (your slot)
    char hostile;        // Hostile flags
    char swar;           // War status
    char armies;         // Armies on ship (if carrying)
    char tractor;        // Visible tractor beam status
    char pad2;
    char pad3;
    u_int flags;         // Player flags
    LONG damage;         // Hull damage (network byte order)
    LONG shield;         // Shield strength
    LONG fuel;           // Fuel amount
    short etemp;         // Engine temperature
    short wtemp;         // Weapon temperature
    short whydead;       // Why player died (if dead)
    short whodead;       // Who killed player
};
```

**SP_PLAYER** (Server to Client, Type 4)
```c
struct player_spacket {
    char type;           // 4
    char pnum;           // Player number
    u_char dir;          // Direction (0-255)
    char speed;          // Ship speed
    LONG x, y;           // Position coordinates (network byte order)
};
```

## UDP Connection

UDP can be negotiated for better performance (lower latency, but unreliable delivery).

### Connection Negotiation

1. Client sends `CP_UDP_REQ` packet requesting UDP mode
2. Server responds with `SP_UDP_REPLY` containing the UDP port number
3. Both client and server switch to using UDP for game updates

### Packet Format

Each UDP datagram contains one or more netrek packets laid end-to-end, similar to TCP:

```
+-----+----------+-----+----------+-----+----------+
| Datagram Payload                                |
+-----+----------+-----+----------+-----+----------+
| pkt | data ... | pkt | data ... | pkt | data ... |
+-----+----------+-----+----------+-----+----------+
```

### Sequence Numbers

UDP packets include sequence numbers appended to allow detection of packet loss and out-of-order delivery:

```c
struct sequence_spacket {
    char type;           // SP_SEQUENCE (29) or SP_SC_SEQUENCE (30)
    u_char flag8;        // Flags
    u_short sequence;    // Sequence number (network byte order)
};
```

The sequence number is included to keep critical and non-critical data synchronized between client and server, as UDP does not guarantee ordered delivery.

### Dual Channel Operation

When both TCP and UDP are in use:
- UDP carries most game update packets (SP_PLAYER, SP_TORP, etc.) for low latency
- TCP carries reliable packets and serves as a backup
- Sequence numbers help correlate packets sent on both channels

## Connection Lifecycle

### Initial Connection (Before Login)

```
Client                              Server
  |                                   |
  +-- CP_SOCKET (version) ----------->|
  |                                   |
  +-- CP_FEATURE (optional) --------->|
  |                                   |
  |<------ SP_MOTD (multiple lines) --+
  |                                   |
  |<------ SP_FEATURE (if CP_FEATURE) +
  |                                   |
  |<------ SP_QUEUE (if queue exists) +
  |                                   |
  |<----------- SP_YOU (slot assign) -+
  |                                   |
```

### Login

```
Client                              Server
  |                                   |
  +------ CP_LOGIN (name/pass) ------>|
  |                                   |
  |<------- SP_LOGIN (accepted?) -----+
  |                                   |
  |<------- SP_YOU (updated) ---------+
  |                                   |
  |<----- SP_PLAYER_INFO (team info) -+
  |                                   |
  |<----- various other packets ------+
  |                                   |
```

### In-Game Play

```
Client                              Server
  |                                   |
  +--- CP_DIRECTION (bearing) ------->|
  +--- CP_SPEED (throttle) ---------->|
  +--- CP_PHASER (attack) ----------->|
  +--- CP_TORP (fire torpedo) ------->|
  +--- CP_MESSAGE (chat) ------------>|
  |                                   |
  |<----- SP_PLAYER (all positions) --+
  |<----- SP_TORP (torpedo positions) +
  |<----- SP_PLASMA (plasma positions)+
  |<----- SP_PHASER (phaser attacks) -+
  |<----- SP_KILLS (kill updates) ----+
  |<----- SP_MESSAGE (chat received) -+
  |                                   |
  |  [continuous updates at ~10Hz]   |
  |                                   |
```

### Disconnection

```
Client                              Server
  |                                   |
  +------- CP_QUIT (self-destruct) -->|
  |     or CP_BYE (quit cleanly)      |
  |                                   |
  |<------- SP_PSTATUS (PDEAD) -------+
  |                                   |
  |     [connection may close]        |
  |                                   |
```

## Data Byte Ordering

All multi-byte integer values in packets are transmitted in **network byte order** (big-endian):
- `LONG` (4 bytes) - 32-bit signed integer
- `U_LONG` (4 bytes) - 32-bit unsigned integer
- `short` (2 bytes) - 16-bit signed integer
- `u_short` (2 bytes) - 16-bit unsigned integer
- `u_int` (4 bytes) - 32-bit unsigned integer

Single-byte fields (`char`, `u_char`) do not need byte-order conversion.

## Timing and Updates

The server updates game state and sends packets at a rate consistent with the configured update rate (typically ~10Hz or 100ms intervals). The update rate can be adjusted by clients via `CP_UPDATES` packets.

The server alternates between:
1. Processing client input packets
2. Waiting for the next timer event
3. Broadcasting updated game state to all connected clients

## Feature Negotiation

The `CP_FEATURE` and `SP_FEATURE` packets allow clients and servers to negotiate support for optional protocol features:

```c
struct feature_cpacket {
    char type;           // CP_FEATURE (60)
    // followed by variable feature flags
};
```

This allows both client and server to communicate which optional extensions they support, enabling protocol evolution while maintaining backward compatibility.

## Additional References

- `include/packets.h` - Complete packet structure definitions
- `ntserv/socket.c` - TCP/UDP socket handling
- `ntserv/genspkt.c` - Packet generation
- `docs/multicast-server-discovery` - Detailed multicast discovery documentation
- `netrek-client-cow/` - Reference client implementation
- `gytha/` - Alternative (pygame) client implementation

## Notes

- The protocol was originally designed in 1989 and has been extended multiple times while maintaining backward compatibility
- TCP provides reliable game state delivery while UDP provides low-latency updates
- All packets are binary and must be parsed according to their type-specific structure
- Clients must be prepared to handle packets arriving in any order and out-of-order UDP packets
- The protocol supports both tournament mode (with masked/hidden information) and casual play modes

## Implementation Conflicts

This section records divergences found by comparing the implementations in
this server (`ntserv/`, `tools/`, `include/`) against the two reference clients
(`netrek-client-cow/`, `gytha/`) and the metaserver
(`netrek-metaserver/`). The metaserver↔client protocol is out of scope.

The **core game protocol is consistent** across all three parties and is not a
source of conflict:

- Packet type numbers agree (server `include/packets.h` vs
  `netrek-client-cow/packets.h`; the only differences are feature-gated or
  dead "ATM" types such as `SP_SCAN`/`SP_FLAGS_ALL` and the special-build
  `CP_OGGV`).
- The fixed-size packet structs are byte-identical, so the server size table
  (`ntserv/genspkt.c` `sizes[]`), the COW handler table
  (`netrek-client-cow/socket.c` `handlers[]`), and the pygame `struct.calcsize`
  table (`gytha/packets.py`) all agree (e.g. `SP_YOU`=32,
  `SP_LOGIN`=104, `SP_PLAYER`=12).
- Version constants match: `SOCKVERSION`=4, `UDPVERSION`=10.
- The server defaults to old fixed-size packets (`send_short` starts 0 in
  `ntserv/socket.c`); short/variable packets are strictly opt-in via
  `CP_S_REQ`/`CP_FEATURE`, so a client that never negotiates them (e.g. pygame,
  which has no short-packet support at all) is never sent one.
- The server→metaserver solicitation (`ntserv/solicit.c`, newline-separated,
  version byte `"b"`) matches the metaserver parser field-for-field, including
  the per-player records (`netrek-metaserver/scan.c` `uread()`).

The genuine conflicts are all in the periphery (server discovery and the
metaserver). The two metaserver items below (#2 and #3) have since been fixed
in `netrek-metaserver`; the client-side multicast limitation (#1) and the
`SP_GENERIC_32` caveat (#4) remain:

### 1. Multicast discovery: clients ignore the port count and drop extra ports

The server's multicast/direct discovery reply is
`s,type,comment,PORTS,port,players,queue[,port,players,queue…]`, where `PORTS`
is a count and an INL server emits **two** port triples
(`tools/players.c` `udp()`, the `"s,%s,%s,2,4566,…,4577,…"` reply at
`tools/players.c:264`; single-port `"…,1,2592,…"` at `:269`).

Neither client reads the count or any triple beyond the first:

- COW `netrek-client-cow/parsemeta.c` `version_s()` reads the count field then
  discards it — `// int ports = atoi(p); /* not currently used */` and
  `// TODO: accept more than one port reply` (`parsemeta.c:535`–`539`) — and
  parses only the first `(port,players,queue)` (and does not even read the
  queue).
- pygame `gytha/meta.py` `version_s()` indexes the split
  list at fixed positions `type=[1]`, `comment=[2]`, `port=[4]`,
  `players=[5]`, `queue=[6]`, skipping the count at `[3]` entirely
  (`meta.py:72`–`76`).

**Effect:** an INL server discovered via multicast is only half-listed — only
the first (home, 4566) port is seen; the away port (4577) is invisible. This
matches the "Known Bugs" note in `docs/multicast-server-discovery`
("doesn't properly handle observer ports for INL servers"). Non-INL servers
(one port) are unaffected.

### 2. Metaserver binary probe assumed 32-bit `long` (fixed in netrek-metaserver)

Besides the UDP solicitation, the metaserver can act as a client and probe a
server's game port, parsing the binary packet stream to count players/queue
(`netrek-metaserver/server.c`, `handlers[]` at `server.c:106`
sized from `sizeof(struct *_spacket)`, consumed by
`scan_packets()` which advances the buffer by `handlers[type].size`
at `server.c:656`/`:679`).

The server keeps its 32-bit wire fields portable: `include/config.h:374`
does `#if (SIZEOF_LONG == 8) #define LONG int` (and `U_LONG u_int`), so
`SP_YOU`/`SP_STATUS`/`SP_LOGIN` fields stay 4 bytes on 64-bit builds. Both
clients do the same — COW at `netrek-client-cow/config.h:293` (identical
`SIZEOF_LONG` guard) and pygame via fixed-width `!…l…` formats (Python's `!`
forces standard 4-byte `l`).

The metaserver's `packets.h` **lacked** that guard: it declared the wire
fields as raw C `long`/`unsigned long`
(`you_spacket` had `long damage; long shield; long fuel;`, `status_spacket`
had `unsigned long timeprod;`), and its Makefile builds native 64-bit
(`CFLAGS = -O2 …`, no `-m32`). On an LP64 build those fields became 8 bytes, so
`sizeof(struct you_spacket)` was ~48 while the server transmits 32.

**Effect:** on a 64-bit metaserver build the binary probe desynchronised at the
first packet containing a `long` field (an `SP_YOU` arrives early in the login
burst: 32 bytes on the wire, but the probe advanced by its inflated
`sizeof`), corrupting the parse of everything after it. It worked only when the
metaserver was built 32-bit (where `long` == `int` == 4 bytes), which is how it
historically ran. The UDP solicitation path (the newline-separated `"b"` format
noted at the top of this section) is text-based and word-size independent, so a
server that solicits was unaffected; the hazard was specifically the active
binary probe of a non-soliciting server.

**Fixed in netrek-metaserver:** the wire structs now use the file's own
`INT32`/`CARD32` typedefs (4 bytes on both ILP32 and LP64, as the newer `*2`
structs already did), so `sizeof(struct you_spacket)` is again 32 and the probe
stays in sync on 64-bit builds. The clients were already correct and needed no
change.

### 3. Metaserver decoded only 20 player slots (fixed in netrek-metaserver)

The server supports `MAXPLAYER` = 32 (`include/defs.h:77`) and encodes each
player's slot with `shipnos = "0123456789abcdefghijklmnopqrstuvwxyz"`
(`ntserv/data.c:62`, used in `ntserv/enter.c:158`), so players in slots 20–31
carry the map characters `k`–`v` in the solicitation player list.

The metaserver's decode table had only 20 entries,
`shipnos[] = {'0'..'9','a'..'j'}` (`netrek-metaserver/server.c:53`,
`NUM_SHIPNOS`=20 in `meta.h:236`), and its player-list loop aborted on the first
slot character it could not map:
`for (i=0;i<NUM_SHIPNOS;i++) if (shipnos[i]==p[0]) break; if (i==NUM_SHIPNOS) break;`
(`scan.c:1152`–`1153`). Its own `SERVER`/`PLAYER` arrays were already sized for
`MAX_PLAYER`=36 with the comment "must be >= server MAXPLAYER"
(`meta.h:85`), so the array was large enough but the decode table was not.

**Effect:** for a solicitation from a server with active players in slots ≥ 20,
the metaserver stopped parsing the player list at the first such record,
dropping those players from its per-server detail. The aggregate counts
(players, queue, comment) parsed earlier were unaffected.

**Fixed in netrek-metaserver:** `shipnos[]` and `NUM_SHIPNOS` were extended to
the full 36 slot characters (`'0'..'9','a'..'z'`), matching the server; the
`shipnos_len_check` compile-time assertion keeps the table and the count in
step.

### 4. `SP_GENERIC_32` version A uses host byte order (negotiated caveat)

`SP_GENERIC_32` (type 32) carries two on-wire layouts: version `'a'` reads its
`u_short` fields in **host** byte order (server `include/packets.h:1104`
`generic_32_spacket_a`, py-struct `"b1sHH26x"` with no leading `!`; identical
note in `netrek-client-cow/packets.h:1247`), while version `'b'` is packed and
uses **network** byte order (`"!b1sHbHBBsBsBB18x"`). This is not a live bug
because the version is negotiated: a client advertises the highest version it
understands via `CP_FEATURE`, and the server sends no higher
(`include/packets.h:1131`–`1149`). It is called out only as a hazard for any
new client — the field byte order depends on the negotiated version, and pygame
sidesteps it by treating the packet as opaque padding (`"!b1s30x"`,
`gytha/packets.py`).

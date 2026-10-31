# Netrek Server

This is the Vanilla Netrek Server.

## Install Dependencies

```
apt install libtool libgdbm-dev
```

## Clone and Build

```
git clone https://github.com/quozl/netrek-server.git
cd netrek-server
./autogen.sh
./configure --prefix=`pwd`/here
make
make install
```

## Starting

```
here/bin/netrekd
```

## Connecting

```
netrek-client-cow
```

## Stopping

```
here/bin/netrekd stop
```

This only stops listening for connections.  A game in progress keeps running, including any robots.  To end it as well:

```
pkill -f ^netrek-daemon
```

## Playing against robots

Pre-T entertainment gives you a 4 on 4 game against the advanced robots from `robotd/` until there are enough humans for a real game.

Set these in `here/etc/sysdef`:

```
PRET=1
IS_ROBOT_BY_HOST=0
```

`IS_ROBOT_BY_HOST=0` is needed if you play on the same computer as the server.  Otherwise a client connecting from `ROBOTHOST` (127.0.0.1) is taken for a robot, and robots only start for humans.

Robots start after a human joins, and within half a minute there are seven of them.  They leave to make room as more humans join.  Once started they keep playing after the humans have gone, until you stop the game.

Each robot reads commands from `here/etc/og/og`, which is not installed.  Without it each robot logs a harmless error to `here/var/ERRORS`, so create it, even if empty:

```
mkdir -p here/etc/og
touch here/etc/og/og
```

Commands go one per line.  For example `hm 9` makes the robots easier, and `hm 0` is the hardest and the default.  The full list is in `robotd/dmessage.c`.

## What is Netrek?

Netrek is the probably the first video game which can accurately be described as a "sport." It has more in common with basketball than with arcade games or Quake. Its vast and expanding array of tactics and strategies allows for many different play styles; the best players are the ones who think fastest, not necessarily the ones who twitch most effectively. It can be enjoyed as a twitch game, since the dogfighting system is extremely robust, but the things that really set Netrek apart from other video games are the team and strategic aspects. Team play is dynamic and varied, with roles constantly changing as the game state changes. Strategic play is explored in organized league games; after more than six years of league play, strategies were still being invented and refined. 

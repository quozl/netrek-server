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

## What is Netrek?

Netrek is the probably the first video game which can accurately be described as a "sport." It has more in common with basketball than with arcade games or Quake. Its vast and expanding array of tactics and strategies allows for many different play styles; the best players are the ones who think fastest, not necessarily the ones who twitch most effectively. It can be enjoyed as a twitch game, since the dogfighting system is extremely robust, but the things that really set Netrek apart from other video games are the team and strategic aspects. Team play is dynamic and varied, with roles constantly changing as the game state changes. Strategic play is explored in organized league games; after more than six years of league play, strategies were still being invented and refined. 

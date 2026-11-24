# Known defects

Defects in netrek-server that are known but not yet fixed.  Older
reports are also kept as ditz issues in `bugs/`.  Future changes are
listed in `PROJECTS`.

## Game

- Twarping to a base while docked on a base can occasionally cause a
  glitch where both bases head warp 60 off the galaxy map.
- Triple mode blanks planet names.  If the server then votes to
  dogfight or hockey, the names are not restored.
- With `surrenderstart=1`, "The 0 0 20 minutes before empire
  collapses" messages are occasionally seen upon neuting a planet.
- Sturgeon bug: a problem with the torp or plasma array, in which a
  player may sometimes get to fire 9 torps, the 9th being in someone
  else's array (it's a different colour torp).  On a related note,
  some people have reported losing the ability to fire torps,
  sometimes completely, sometimes in increments of 2, such as 6 or 4
  or 2.  [Probably fixed; see the conquer ring plasma bug.]
- Sending SIGSTOP to a client for 35 seconds results in a ghostbust,
  which is good, but the client's response after SIGCONT is not good:
  it presents the team selection window, and clicking on a team hangs
  the client.
- Find the causes of jammed slots, and expire them sooner.
- Jammed slots on other teams?  Can they be ejected?
- Player "^%^" took a planet and it said "Planet taken by ^".
  Reported by zu22 (Zach Uram) on 2006-04-05.  This is also a client
  issue.
- A god observer should see all teams' messages.
- Switching into or out of no-geno timer hours on continuum resets the
  quit timeout, perhaps due to a repeat of RSA verification.
- Change metaserver solicitation to process multiple IP addresses from
  each DNS host name given, just like the new COW metaserver code.
  [Coding in progress by cameron@stl.dec.com.]

## INL

- On first entry to an `INL=1` server, "INL->X0 You have been ejected
  due to player DB inconsistency." is shown, but it does not hinder
  the player or the game.
- `INL=1` is incompatible with `FPS=50`.  The workaround is to set
  `FPS=10`.
- The email address that INL statboys is sent to is defunct, and
  messages bounce.
- Quitting as captain in an INL game should automatically uncaptain.
- Team masks are switched at the end of an INL game.  Is there any
  particular reason why we're doing this?

## Robots

- Robots are not recognising Cas.  Reported by KaraokeBar, Australia,
  on 2007-05-15.
- Since williamb's "Improved tracking of server modes" (200ce40),
  `status->gameup` synchronisation with forked robots includes race
  conditions and an unsafe process.  A robot sets the flag once
  started; it might not complete starting by the time an attacker
  requests another vote.  If a robot terminates unnaturally, the flag
  is left set, preventing a server reset or another vote.
- Practice robots fail to shoot torpedoes if created while the server
  is in topgun mode.
- Practice robots drop shields as soon as you die, thus taking torps
  in flight on hull.
- cambot does not exit when the daemon exits.

## Build and install

- `make clean` in `ntserv` doesn't.
- configure's "checking for BSD style signals" doesn't finish with a
  yes or a no.
- Check the dependencies in `ntserv` by hand.  An installer found
  that a `defs.h` change was not picked up by at least one thing
  there, and so a `make clean` was required.
- If an installer changes `config.h.in` and then `config.h` by hand,
  running `make` causes configure to be re-run, but without the
  appropriate `--prefix`, thus building for the wrong place.
- configure should set `CONFIRM=1` if it finds res-rsa, and the
  Makefile should recommend running `updated` at least once.
- The `xsg` Makefile (and perhaps others) compiles
  `../ntserv/getpath.c`, saving the object file into the `xsg`
  directory, and then promptly uses the object files from the
  `ntserv` directory, regardless of whether they are built yet.  It
  should either use the local copy (and so waste compile time), or
  nest into the `ntserv` Makefile.

## Review feedback

Recorded with the 2.18.0 prerelease in 2011, from IRC:

> \<Gerdesas\> I must say I don't concur with some of these changes.
> "Your team has prohibited you beaming up" is not concise for new
> players; there is no rhyme nor reason to parts of the code base that
> use TRUE/FALSE and those that do not.  TRUE and FALSE make for more
> readable code and on top of that there was no such review push for
> my original work on this that added TRUE/FALSE to other files.
> TRUE/FALSE
>
> \<Gerdesas\> should be defined in in a top-level include, also.

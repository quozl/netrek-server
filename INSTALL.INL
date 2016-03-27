Instructions for installing the Vanilla Netrek Server as an INL Server

Summary
-------

	0)  choose a place to put the server (e.g. /usr/local/games/netrek)
	1)  type "./configure --prefix=dir --with-inl" where dir is from step 0
	2)  type "make install"
	3)  ensure no conflict in ports with your pickup server if any
	4)  "bin/netrekd" to start the server
	5)  use a client to connect to the server on port 4566/4577
	6)  list home/away ports on metaserver (optional)
	7)  modify auto-archive.pl and end_tourney.pl scripts as necessary

Details
-------

See INSTALL for a complete description.  This file describes only the stuff
peculiar to installing the INL Server mode.

0) Don't use the same directory as an existing pickup server.

1) No comment.

2) No comment.

3) If you have a pickup server, you must be sure to remove all the INL
ports from the ports file in the pickup server's directory, and
restart netrekd if you made a material change.  You must also remove
the pickup and player list ports from the ports file of the INL
server.  If you don't do this, the symptom is a failure of the netrekd
process to get going.

4) Yes, you end up with two netrekd processes on your system if you
have a pickup server.  As of 2.9pl0, netrekd wrote its process id into
the server directory as netrekd.pid, so determining which is which
should not be a problem.

5) On slow systems, a second client connection may be required.  This
may be a race condition between the startup of the client and the
server startup and subsequent opening of the queues by the INL robot.
The symptom is a "Sorry, but you cannot play Xtrek right now" or
similar.

6) See docs/metaservers-HowTo for a way to list player ports on the
metaserver.  Most clients assume the observer port is +1 port higher
than the player port, so using the standard 4566/4000 home and
4577/5000 away ports will cause problems with connecting to observe
through the metaserver window.  One solution is to instead use
4566/4567 home and 4577/4578 away ports.

7) Modify robots/end_tourney.pl if you wish to change the directory
into which INL files are dropped.  Modify robots/auto-archive.pl if
you wish to specify server name (default uses FQDN, which may or may
not work depending on your server).  During install these scripts
are copied into the tools directory.

For assistance, write to netrek-dev@us.netrek.org

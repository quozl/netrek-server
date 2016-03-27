Summary: Netrek Software Suite
Name: netrek
Version: 2.18.0
Release: 0
Copyright: Undetermined
Packager: Vanilla Server Development Team
URL: http://vanilla.us.netrek.org/
Group: Amusements/Games
Source0: ftp://ftp.netrek.org/pub/netrek/servers/vanilla/Vanilla-%{version}.tar.gz
Source1: netrek.init
Source2: netrek.logrotate
Source3: netrek.crontab
Source4: netrek.keys
Source5: netrek.functions
Source6: netrek.gnome
Source7: netrek.png

BuildRoot: /var/tmp/netrek-buildroot

%description
Netrek is the probably the first video game which can accurately be
described as a "sport."  It has more in common with basketball than
with arcade games or Quake.  Its vast and expanding array of tactics
and strategies allows for many different play styles; the best players
are the ones who think fastest, not necessarily the ones who twitch
most effectively.  It can be enjoyed as a twitch game, since the
dogfighting system is extremely robust, but the things that really set
Netrek apart from other video games are the team and strategic
aspects.  Team play is dynamic and varied, with roles constantly
changing as the game state changes.  Strategic play is explored in
organized league games; after 6+ years of league play, strategies are
still being invented and refined.

%package server
Summary: Netrek Vanilla Server
Group: Amusements/Games
Prereq: sed, vixie-cron, sh-utils
Requires: vixie-cron, sed, sh-utils, redhat-release > 5.9

%description server
This is a server for the multi-player game of Netrek.

Netrek is the probably the first video game which can accurately be
described as a "sport."  It has more in common with basketball than
with arcade games or Quake.  Its vast and expanding array of tactics
and strategies allows for many different play styles; the best players
are the ones who think fastest, not necessarily the ones who twitch
most effectively.  It can be enjoyed as a twitch game, since the
dogfighting system is extremely robust, but the things that really set
Netrek apart from other video games are the team and strategic
aspects.  Team play is dynamic and varied, with roles constantly
changing as the game state changes.  Strategic play is explored in
organized league games; after 6+ years of league play, strategies are
still being invented and refined.

The game itself has existed for over 10 years, and has a solid
playerbase, including some people who have been playing for nearly as
long as the game has existed.

All Netrek clients and servers are completely free of charge, although
there are several people working on commercial netrek variants or
derivatives.

Netrek web site:          <http://www.netrek.org/>
Development mailing list: <mailto:vanilla-list@us.netrek.org>
Development web site:     <http://vanilla.us.netrek.org/>
#
# GUI Admin tool
#
%package config
Summary: Netrek Vanilla Server Configuration Program
Prereq: automake
Requires: netrek-server, gtk+ > 1.2, glib > 1.2,  redhat-release > 5.9
Group: Amusements/Games

%description config
Configuration program for the Netrek Vanilla Server.

Although the server will run out of the box, it can be configured to
behave differently through configuration files.  This program provides
a graphical interface to these files.  It is not required to play.
%prep

%setup -n Vanilla-%{version}

%build

#
# Look for the US RSA stuff.
#
if [ -f $RPM_SOURCE_DIR/res-rsa-2.9.2.tar.gz ]; then
  RSA_FILE=$RPM_SOURCE_DIR/res-rsa-2.9.2.tar.gz
  RSA_RELEASE=2.9.2

#
# Look for the Euro RSA stuff.
#
elif [ -f $RPM_SOURCE_DIR/res-rsa-2.9.tar.gz ]; then
  RSA_FILE=$RPM_SOURCE_DIR/res-rsa-2.9.tar.gz
  RSA_RELEASE=2.9

#
# Could not find either, does the user have the file encrypted?
#
elif [ -f $RPM_SOURCE_DIR/res-rsa-2.9.1.tar.gz.crypt ]; then
  echo "Found RSA files, but they are encrypted, send email to rsakeys@us.netrek.org"
  echo "for the password to decrypt the file. If you want to build the server without"
  echo "RSA, remove $RPM_SOURCE_DIR/res-rsa-2.9.1.tar.gz.crypt and run rpm again."
  exit 1

#
# No RSA code, so just tell the user we are going to compile with RSA
#
else
  echo "No RSA source code found. I am going to compile the server without RSA."
fi

#
# Extract the RSA source if we found it
#
if [ ! -z $RSA_FILE ]; then
  ln -s res-rsa res-rsa-$RSA_RELEASE
  zcat $RSA_FILE | tar -xvf -
fi

#
# Build the netrek server
#
./configure --prefix=/usr/games/netrek
make

#
# Build the netrek server control GUI
#
cd gum
./configure --prefix=/usr/games/netrek
make

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT/etc/logrotate.d
install -d $RPM_BUILD_ROOT/etc/rc.d/init.d
install -d $RPM_BUILD_ROOT/etc/cron.hourly
install -d $RPM_BUILD_ROOT/var/log/netrek
install -d $RPM_BUILD_ROOT/usr/bin
install -d $RPM_BUILD_ROOT/usr/share/games/netrek
install -d $RPM_BUILD_ROOT/usr/share/gnome/apps/Games
install -d $RPM_BUILD_ROOT/usr/share/pixmaps

#
# Use INSTALLOPTS="-s" if you want the binaries stripped.
#
make LIBDIR=$RPM_BUILD_ROOT/usr/games/netrek install
install -m 0644 $RPM_SOURCE_DIR/netrek.logrotate $RPM_BUILD_ROOT/etc/logrotate.d/netrek
install -m 0555 $RPM_SOURCE_DIR/netrek.init $RPM_BUILD_ROOT/etc/rc.d/init.d/netrek
install -m 0644 $RPM_SOURCE_DIR/netrek.functions $RPM_BUILD_ROOT/etc/rc.d/init.d/functions-games
install -m 0555 $RPM_SOURCE_DIR/netrek.crontab $RPM_BUILD_ROOT/etc/cron.hourly/netrek
install -m 0644 $RPM_SOURCE_DIR/netrek.keys $RPM_BUILD_ROOT/usr/games/netrek/rsa-keyfile

#
# Install GNOME menu items
#
install -m 0444 $RPM_SOURCE_DIR/netrek.gnome $RPM_BUILD_ROOT/usr/share/gnome/apps/Games/netrekcfg.desktop
#install -m 0444 $RPM_SOURCE_DIR/netrek.png $RPM_BUILD_ROOT/usr/share/pixmaps/netrekd.png

#
# Create the ghosts. The %post will do the actual symlink on install.
#
touch $RPM_BUILD_ROOT/usr/bin/netrekd
touch $RPM_BUILD_ROOT/usr/bin/netrekcfg

#
# Any file that the server creates needs to be added here so I can
# touch the file in the /usr/games/netrek directory. Also don't forget
# to add a %ghost entry for each of these files. This is necessary so if
# a user removes the server all files, even one generated from the
# server are removed.
#
for f in rsa_key log ERRORS motd_list .global .newstartd.pid .players \
	god.LOG logfile rsa-keyfile~ tools/pixmaps mesg.LOG; do
  touch $RPM_BUILD_ROOT/usr/games/netrek/$f
done

#
# Any new log files should be added here and the appropriate %ghost
# entries in the file section.
#
for f in god.LOG mesg.LOG newstartd.LOG updated.LOG errors.LOG; do
  touch $RPM_BUILD_ROOT/var/log/netrek/$f
done

#
#  Install the netrek server control GUI
#
cd gum
make install prefix=$RPM_BUILD_ROOT/usr/games/netrek \
	bindir=$RPM_BUILD_ROOT/usr/games/netrek/tools \
	datadir=$RPM_BUILD_ROOT/usr/share/games/netrek

%clean
rm -rf $RPM_BUILD_ROOT

%post server
chkconfig --add netrek

#
# James wants netrekd int /usr/bin, so symlink
#
ln -s /usr/games/netrek/newstartd /usr/bin/netrekd

#
# Server normally writes logs to LIBDIR, symlink all log files to
# /var/log/netrek to keep with the linux standards. Make sure you
# perform the symlink as user games.
#
su -c "ln -s /var/log/netrek/newstartd.LOG /usr/games/netrek/log" - games
su -c "ln -s /var/log/netrek/god.LOG /usr/games/netrek/god.LOG" - games
su -c "ln -s /var/log/netrek/newstartd.LOG /usr/games/netrek/logfile" - games
su -c "ln -s /var/log/netrek/errors.LOG /usr/games/netrek/ERRORS" - games
su -c "ln -s /var/log/netrek/mesg.LOG /usr/games/netrek/mesg.LOG" - games

%preun server

echo "You might receive several messages that state:"
echo "   removal of 'filename' failed: No such file or directory"
echo "You can ignore these messages. Thanks."

chkconfig --del netrek

if [ -f /var/lock/subsys/netrek ]; then
/etc/rc.d/init.d/netrek stop
fi

%postun server

%post config
#ln -s /usr/games/netrek/tools/netrekcfg /usr/bin/netrekcfg
ln -s /usr/games/netrek/tools/gum /usr/bin/netrekcfg
su -c "ln -sf /usr/share/games/netrek/gum/pixmaps /usr/games/netrek/tools/pixmaps" - games

%files server
%defattr(-, root, root)
%doc PROJECTS README INSTALL INSTALL.INL docs tools/README xsg/README 
%doc xsg/XSG.doc xsg/Sample.xsgrc pledit/README
/etc/logrotate.d/netrek
/etc/rc.d/init.d/netrek
/etc/rc.d/init.d/functions-games

%defattr(-, games, games)
%attr(2755, games, games) %dir /usr/games/netrek
%attr(2755, games, games) %dir /var/log/netrek
%ghost /var/log/netrek/god.LOG
%ghost /var/log/netrek/mesg.LOG
%ghost /var/log/netrek/updated.LOG
%ghost /var/log/netrek/newstartd.LOG
%ghost /var/log/netrek/errors.LOG
%config /usr/games/netrek/.global
%config /usr/games/netrek/.players
%config /usr/games/netrek/.planets
%config /usr/games/netrek/.features
%config /usr/games/netrek/.motd_clue
%config /usr/games/netrek/motd_clue_logo.MASTER
%config /usr/games/netrek/motd_basep_logo.MASTER
%config /usr/games/netrek/.sysdef
%config /usr/games/netrek/.motd
%config /usr/games/netrek/motd_logo.MASTER
%config /usr/games/netrek/.ports
%config /usr/games/netrek/.time
%config /usr/games/netrek/.motd_basep
%config /usr/games/netrek/.tourn.map
%config /usr/games/netrek/.nocount
%config /usr/games/netrek/.banned
%config /usr/games/netrek/.bypass
%config /usr/games/netrek/.clue-bypass
%config /usr/games/netrek/.reserved
/etc/cron.hourly/netrek
/usr/games/netrek/rsa-keyfile
%ghost /usr/games/netrek/mesg.LOG
%ghost /usr/games/netrek/rsa-keyfile~
%ghost /usr/games/netrek/god.LOG
%ghost /usr/games/netrek/logfile
%ghost /usr/games/netrek/log
%ghost /usr/games/netrek/rsa_key
%ghost /usr/games/netrek/motd_list
%ghost /usr/games/netrek/ERRORS
%ghost /usr/games/netrek/.newstartd.pid
/usr/games/netrek/ntserv
/usr/games/netrek/daemon
/usr/games/netrek/puck
/usr/games/netrek/mars
/usr/games/netrek/robotII
/usr/games/netrek/basep
/usr/games/netrek/inl
/usr/games/netrek/end_tourney.pl
/usr/games/netrek/newstartd
%ghost /usr/bin/netrekd
/usr/games/netrek/netrekd
/usr/games/netrek/updated
/usr/games/netrek/cambot
/usr/games/netrek/rsa_keycomp
/usr/games/netrek/rsa_key2cap
/usr/games/netrek/pledit
/usr/games/netrek/sequencer
/usr/games/netrek/xsg
/usr/games/netrek/trekon
/usr/games/netrek/trekon.bitmap
/usr/games/netrek/trekoff.bitmap
/usr/games/netrek/tools
#/usr/share/pixmaps/netrekd.png

%files config
%defattr(-, games, games)
%ghost /usr/bin/netrekcfg
%attr(4111, games, games)/usr/games/netrek/tools/gum
/usr/share/games/netrek
/usr/share/gnome/apps/Games/netrekcfg.desktop
%ghost /usr/games/netrek/tools/pixmaps

%changelog
* Fri Jul 16 1999  Bob Tanner  <tanner@real-time.com>

	- rpm/Vanilla.spec (crontab): Changed the .spec file to use James'
	elegant fix to the cron problem.

	- rpm/Vanilla.spec (logfiles):  Added updated.LOG to keep binaries
	to logfiles synchronized.

	- rpm/Vanilla.spec (logfiles): Changed start.LOG to newstartd.LOG
	to keep the relationship very clear.

* Thu Jul 15 1999  Bob Tanner  <tanner@real-time.com>

	- rpm/Vanilla.spec (Requires): Changed cron to vixie-cron.  

	- rpm/Vanilla.spec (%files): Changed the %files section for the
	server. Needed to break out each file so I could tag certain files
	as %config.

	- rpm/Vanilla.spec (clean): Added %clean to rm -f the
	BUILD_ROOT.

	- rpm/Vanilla.spec (US RSA): Added logic to look for the RSA
	source. If it is found, extract into the right directory. The
	configure script can only detect the US RSA source, it looks for
	res-rsa/configure, which is not in the Euro version.

* Thu Jul 15 1999  Bob Tanner  <tanner@real-time.com>

	- rpm/Vanilla.spec (Source1): Had to remove the
	ftp://ftp.risc.uni-linz.ac.at/pub/netrek/src/res-rsa2.tar.Z file,
	since US crypto law sucks and I cannot redistribute it. Still
	working on how to build the server with RSA enabled and not
	distribute the source for it.

	- rpm/Vanilla.spec (%postun): Major hack again. Need to get netrek
	cron entries out of games' crontabs without deleting other
	potential crontab entries. ** NOTICE ** this method required that
	"netrek" (the word) be part of the crontab entries that need to be
	removed. This might come back and bite us in the butt.

	- rpm/Vanilla.spec (%post): Major hack to get cron jobs into
	games' crontab. Have to be careful, games may already have a
	crontab tab, so suck the current  crontab out and save it to a
	temp location. Append netrek's cron entries to the crontab. Put
	the crontab back into cron. Finally clean up after ourselves.

	- rpm/Vanilla.spec (Requires): The .spec file is becoming more
	complex. We need to have gtk > 1.2, glib > 1.2 for gum. Cron and
	sed are necessary for setting up crontab entries for updated on
	install and deleted those same entries on removal.

* Wed Jul 14 1999  Bob Tanner  <tanner@real-time.com>
	- rpm/Vanilla.spec (Group): RPM %changelog format is different
	then this files format. Changed the .spec files format to keep rpm
	from complaining.

* Wed Jul 14 1999  Bob Tanner  <tanner@real-time.com>
	- rpm/Vanilla.spec (Group): Changed the Source0 from tar.gz to
	tar.bz2 to fall in line with James distributions.

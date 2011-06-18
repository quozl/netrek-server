dnl	This file is an input file used by the GNU "autoconf" program to
dnl	generate the file "configure", which is run during Netrek installation
dnl	to configure the system for the local environment.
AC_INIT(ntserv/daemon.c)
AC_CONFIG_HEADER(include/config.h)
AC_PREFIX_DEFAULT("/usr/local/games/netrek-server-vanilla")

  # default the prefix
  if test "x$prefix" = "xNONE"; then
    prefix=${ac_default_prefix}
  fi
  # expand the other directories
  if test "x${exec_prefix}" = 'xNONE'; then
   exec_prefix=${prefix}
  fi
  if test "x${bindir}" = 'x${exec_prefix}/bin'; then
    bindir=${exec_prefix}/bin
  fi
  if test "x${libdir}" = 'x${exec_prefix}/lib'; then
    libdir=${exec_prefix}/lib
  fi
  if test "x${sysconfdir}" = 'x${prefix}/etc'; then
    sysconfdir=${prefix}/etc
  fi
  if test "x${localstatedir}" = 'x${prefix}/var'; then
    localstatedir=${prefix}/var
  fi
  # place the important ones for use by the C code
  AC_DEFINE_UNQUOTED(BINDIR,"${bindir}")
  AC_DEFINE_UNQUOTED(LIBDIR,"${libdir}")
  AC_DEFINE_UNQUOTED(SYSCONFDIR,"${sysconfdir}")
  AC_DEFINE_UNQUOTED(LOCALSTATEDIR,"${localstatedir}")
  AC_SUBST(LOGNAME)

  AC_ARG_WITH(inl, [  --with-inl              INL Mode ], INL=1, INL=0)
  if test "x$INL" = x1 ; then
	AC_DEFINE(PKEY,129)
	AC_DEFINE(LTD_STATS)
	AC_DEFINE(LTD_PER_RACE)
	AC_SUBST(PKEY)
	AC_SUBST(INL)
  fi

AC_PROG_INSTALL
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_AIX

AC_C_INLINE

#--------------------------------------------------------------------
#	Include sys/select.h if it exists and if it supplies things
#	that appear to be useful.  This appears to be true only on
#	the RS/6000 under AIX.  Some systems like OSF/1 have a
#	sys/select.h that's of no use, and other systems like SCO
#	UNIX have a sys/select.h that's pernicious.  If there isn't
#	a sys/select.h, then make sure that "fd_set" is defined in
#	sys/types.h.
#--------------------------------------------------------------------

AC_MSG_CHECKING(if fd_set requires sys/select.h)
AC_TRY_COMPILE( [#include <sys/types.h>], [fd_set readMask, writeMask;],
  AC_MSG_RESULT(no) ,
  AC_EGREP_HEADER(fd_set, sys/select.h, 
      AC_DEFINE(NEED_SYS_SELECT_H) AC_MSG_RESULT(yes),
      AC_DEFINE(NO_FD_SET) AC_MSG_RESULT(fd_set missing)))

#--------------------------------------------------------------------
#	Check for various typedefs and provide substitutes if
#	they don't exist.
#--------------------------------------------------------------------

AC_HEADER_STDC
AC_CHECK_HEADERS(unistd.h memory.h math.h stdlib.h)
AC_CHECK_HEADERS(sys/timeb.h sys/ptyio.h sys/fcntl.h fcntl.h)
AC_CHECK_HEADERS(ctype.h machine/endian.h sys/resource.h)
AC_CHECK_HEADERS(sys/wait.h netinet/in.h sys/filio.h gdbm.h)
AC_CHECK_HEADERS(ncurses.h)

AC_CHECK_LIB(gdbm, gdbm_open, [LIBS="$LIBS -lgdbm"])

AC_TYPE_PID_T
AC_TYPE_UID_T
AC_TYPE_SIZE_T
AC_FUNC_VFORK
AC_STRUCT_TM

AC_MSG_CHECKING(for itimer in time.h)
AC_EGREP_HEADER(itimerval, time.h, AC_MSG_RESULT(yes) , AC_DEFINE(NEED_SYS_TIME_H) AC_MSG_RESULT(no))

AC_CHECK_SIZEOF(long)

AC_MSG_CHECKING(for u_int in sys/types.h)
AC_EGREP_HEADER(u_int, sys/types.h, AC_MSG_RESULT(yes) , AC_DEFINE(NO_U_INT) AC_MSG_RESULT(no))

AC_MSG_CHECKING(for PATH_MAX in limits.h)
AC_EGREP_CPP(PATH_MAX, [
#include <limits.h> 
PATH_MAX
], AC_DEFINE(NO_PATH_MAX) AC_MSG_RESULT(no), AC_MSG_RESULT(yes))



#--------------------------------------------------------------------
#	Check for gmp and mp
#--------------------------------------------------------------------

AC_ARG_WITH(gmp,[  --with-gmp              GMP Dir], MPINC="-I$withval/include"  MPLIB="-L$withval/lib -lgmp" search=false, MPINC=nope MPLIB=nope)
if test "$MPINC" = nope; then
    AC_MSG_CHECKING(for mp.h)
    AC_EGREP_CPP(GNU_MP,[ 
#include <mp.h> 
__GNU_MP__
],
        [gnu_mp_ok=yes],
        [gnu_mp_ok=no])
    if test "x$gnu_mp_ok" = xyes; then
        MPINC=nope
	search=true
	AC_MSG_RESULT(yes)
    else
        MPINC=""
	search=false
	AC_MSG_RESULT(no)
    fi
fi

AC_MSG_CHECKING(for gmp.h)
#   if test "$MPINC" = nope; then
#      AC_TRY_CPP([#include <gmp.h>], [MPINC="-I/usr/include" search=false], MPINC=nope)
#   fi

if test "$MPINC" = nope; then
   dirs="$srcdir/gmp-2.0.2 /usr/unsupported/include /usr/local/include \
      /usr/gnu/include /usr/gnu/include/mp /usr/local/gnu/include \
      /usr/local/gnu/include/mp /usr/include/gnu /usr/local/lib/gmp-2.0.2"
   for i in $dirs; do
      if test -r $i/gmp.h; then
	MPINC="-I$i"
      fi
   done
fi

if test "$MPINC" = nope; then
  AC_TRY_CPP([#include <mp.h>], MPINC="" , MPINC=nope)
  if test "$MPINC" = nope; then
    MPINC="# -I."
    NORSA="#"
    AC_MSG_RESULT(no)
  else
    AC_MSG_RESULT(yes)
  fi
else
  AC_MSG_RESULT(yes)
fi

AC_SUBST(MPINC)

# Checking for GMP 2
SAVEINCS="$INCS"
INCS="$INCS $XINCLUDES"
AC_EGREP_CPP("gmp=.*2", [
  #include <gmp.h> 
  gmp=__GNU_MP__
  ],AC_DEFINE(HAVE_GMP2_H))
INCS="$SAVEINCS"

if test "$MPLIB" = nope; then
  if test "$search" = true; then
    MPLIB=nope
  else
    AC_CHECK_LIB(mp, main, MPLIB="-lmp", MPLIB=" ")
    AC_CHECK_LIB(gmp, main, MPLIB="$MPLIB -lgmp", MPLIB=nope)
  fi
  if test "$MPLIB" = nope; then
    dirs="$srcdir/gmp-2.0.2 /usr/unsupported/lib /usr/local/lib /usr/gnu/lib \
      /usr/local/gnu/lib /usr/lib/gnu /usr/local/lib/gmp-2.0.2" 
    for i in $dirs; do
      if test -r $i/libmp.a; then
    	MPLIB="-L$i -lmp"
      fi
      if test -r $i/libgmp.a; then
	MPLIB="-L$i -lgmp"
	AC_MSG_RESULT(yes)
      fi
    done
  fi
fi
if test "$MPLIB" = nope; then
  AC_CHECK_LIB(mp, main, MPLIB="-lmp", MPLIB=nope)
  if test "$MPLIB" = nope; then
    echo Warning mp or gmp library not found, turning RSA off.
    MPLIB="# -L. -lmp -lgmp"
    NORSA="#"
  fi
fi
AC_SUBST(MPLIB)

AC_MSG_CHECKING(res-rsa/configure)
if test -r "$srcdir/res-rsa/configure"; then
  AC_CONFIG_SUBDIRS(res-rsa)
  AC_MSG_RESULT(RSA utilities found)
  RSAINC="-I${srcdir}/res-rsa"
  RSALIB="-L`pwd`/res-rsa -lrsa"
  NORSA=""
else
  AC_MSG_RESULT(no)
  echo Warning: RSA utilities for server build not found, turning RSA off
  NORSA="#"
fi

AC_SUBST(RSAINC)
AC_SUBST(RSALIB)
AC_SUBST(NORSA)

#--------------------------------------------------------------------
#	Check for the existence of various libraries.  The order here
#	is important, so that then end up in the right order in the
#	command line generated by Make.
#--------------------------------------------------------------------

AC_CHECK_LIB(Xbsd, main, [LIBS="$LIBS -lXbsd"])
AC_CHECK_LIB(socket, main, [LIBS="$LIBS -lsocket"])
AC_CHECK_LIB(resolv, main, [LIBS="$LIBS -lresolv"])
AC_CHECK_LIB(inet, main, [LIBS="$LIBS -linet"])
AC_CHECK_LIB(nsl, main, [LIBS="$LIBS -lnsl"])
AC_CHECK_LIB(seq, main, [LIBS="$LIBS -lseq"])
AC_CHECK_LIB(sun, main, [LIBS="$LIBS -lsun"])

AC_CHECK_LIB(ipc, main, [LIBS="$LIBS -lipc"])
AC_CHECK_LIB(shm, main, [LIBS="$LIBS -lshm"])
AC_CHECK_LIB(stuff, main, [LIBS="$LIBS -lstuff"])

AC_CHECK_LIB(crypt, crypt, [LIBCRYPT="-lcrypt"])
AC_SUBST(LIBCRYPT)

AC_CHECK_LIB(termcap, main, [LIBTERMCAP="-ltermcap"])
AC_SUBST(LIBTERMCAP)

AC_CHECK_LIB(ncurses, newwin, [LIBCURSES="-lncurses"],
     AC_CHECK_LIB(curses, newwin, [LIBCURSES="-lcurses"])
)
AC_SUBST(LIBCURSES)

AC_CHECK_FUNCS(usleep random setstate strftime ftime strcmpi strncmpi)

AC_CHECK_LIB(m, main, [LIBS="$LIBS -lm"])
AC_CHECK_FUNCS(nint,HAVE_NINT="yes",
		AC_CHECK_LIB(sunmath,nint,HAVE_NINT="-lsunmath",
		HAVE_NINT="no")
	      )

NINT_LIBS="";
if test "x$HAVE_NINT" = "xno"; then
	HAVE_NINT="0"
	AC_SUBST(HAVE_NINT)
else
	if test "x$HAVE_NINT" = "xyes"; then
		NINT_LIBS=${HAVE_NINT}
	fi
	HAVE_NINT="1"
	AC_SUBST(HAVE_NINT)
fi

AC_SUBST(NINT_LIBS)

AC_EGREP_HEADER(rint, math.h, , AC_DEFINE(NEED_RINT_DEC))

AC_REPLACE_FUNCS(random strdup rint)

#--------------------------------------------------------------------
#       Check for system dependend programs
#--------------------------------------------------------------------

AC_PROG_LIBTOOL

AC_MSG_CHECKING(for Mac OS X/Darwin ranlib)
if test "$OSTYPE" != "darwin"; then
    RANLIB_FLAGS=
    AC_MSG_RESULT(no)
else
    RANLIB_FLAGS='-c'
    AC_MSG_RESULT(yes)
fi

AC_SUBST(RANLIB_FLAGS)

AC_OUTPUT(system.mk Makefile ntserv/Makefile tools/Makefile
	sequencer/Makefile newstartd/Makefile robots/Makefile
	keycomp/Makefile robotd/Makefile
	docs/Makefile tools/no_geno_timer tools/geno_timer
	docs/sample_geno_timer_crontab docs/sample_sysdef
	tools/setpath tools/setpath.csh tools/admin/Makefile)

echo ""
echo "Directories"
echo "    Binaries (external use) : $bindir"
echo "    Binaries (internal use) : $libdir"
echo "    Configuration files     : $sysconfdir"
echo "    Local state files       : $localstatedir"
echo ""

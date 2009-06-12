### Required cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.4.0)

### Pretty colors!
SET(CMAKE_COLOR_MAKEFILE ON)

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

### Let me see the command line
SET (CMAKE_VERBOSE_MAKEFILE OFF)

### Debug build please
set(CMAKE_BUILD_TYPE Debug)

# Put the include dirs which are in the source or build tree
# before all other include dirs, so the headers in the sources
# are prefered over the already installed ones
# since cmake 2.4.1
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

project(netrek-server-vanilla C)
set(VERSION "2.15")

### Command line options
OPTION(WITH_ANSI "enforce strict ISO C++ conforming" ON)
OPTION(VERBOSE "show cmake command line" OFF)
OPTION(RSA "build with RSA components on" OFF)

IF (VERBOSE)
	SET (CMAKE_VERBOSE_MAKEFILE ON)
	MESSAGE (STATUS "Show cmake command line")
ENDIF (VERBOSE)


### Our own modules
set(CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/cmake/Modules
)

### Include modules
INCLUDE (DefineInstallationPaths)
INCLUDE (CheckIncludeFile)
INCLUDE (CheckTypeSize) 
INCLUDE (CheckLibraryExists)
INCLUDE (CheckFunctionExists)
#INCLUDE (FindCurses)
#INCLUDE (FindX11)


### Platform specific stuff
IF (UNIX)
	MESSAGE (STATUS "Unix")
	IF (APPLE)
		MESSAGE (STATUS "Apple")
		# create a C static library see http://permalink.gmane.org/gmane.games.netrek.devel/2676 for details
	  	SET(CMAKE_C_CREATE_STATIC_LIBRARY
	      "<CMAKE_AR> cr <TARGET> <LINK_FLAGS> <OBJECTS> "
	      "<CMAKE_RANLIB> -c <TARGET> "
		)
	
		SET (CMAKE_C_LINK_FLAGS 
			-flat-namespace
			${CMAKE_C_LINK_FLAGS}
		)
	ENDIF (APPLE)
ELSE (UNIX)
	IF (WIN32)
		MESSAGE(STATUS "Good luck!")
	ENDIF (WIN32)
ENDIF (UNIX)

### Setup short variable path names
set(BINDIR ${BIN_INSTALL_DIR})
set(SBINDIR ${SBIN_INSTALL_DIR})
set(SYSCONFDIR ${SYSCONF_INSTALL_DIR})
set(LOCALSTATEDIR ${LOCALSTATE_INSTALL_DIR})
set(MANDIR ${MAN_INSTALL_DIR})
set(DATADIR ${DATA_INSTALL_DIR})
set(LIBDIR ${LIB_INSTALL_DIR})

### Type checks
CHECK_TYPE_SIZE ("int" SIZEOF_SIGNED_INT)
CHECK_TYPE_SIZE ("unsigned int" SIZEOF_UNSIGNED_INT)
CHECK_TYPE_SIZE ("signed long" SIZEOF_SIGNED_LONG)
CHECK_TYPE_SIZE ("unsigned long" SIZEOF_UNSIGNED_LONG)
CHECK_TYPE_SIZE ("unsigned long long" SIZEOF_UNSIGNED_LONG_LONG)

### Header checks
CHECK_INCLUDE_FILE (stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILE (sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE (inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE (unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILE (string.h HAVE_STRING_H)
CHECK_INCLUDE_FILE (sys/fcntl.h HAVE_SYS_FCNTL_H)
CHECK_INCLUDE_FILE (sys/select.h HAVE_SYS_SELECT_H)
CHECK_INCLUDE_FILE (sys/wait.h HAVE_SYS_WAIT_H)
CHECK_INCLUDE_FILE (sys/filio.h HAVE_SYS_FILIO_H)
CHECK_INCLUDE_FILE (errno.h HAVE_ERRNO_H)
CHECK_INCLUDE_FILE (crypt.h HAVE_CRYPT_H)

### Function checks (NB: Really needed? )
CHECK_FUNCTION_EXISTS(usleep HAVE_USLEEP) 
CHECK_FUNCTION_EXISTS(wait3 HAVE_WAIT3) 
CHECK_FUNCTION_EXISTS(strncpy HAVE_STRNCPY) 
CHECK_FUNCTION_EXISTS(signal HAVE_SIGNAL) 
CHECK_FUNCTION_EXISTS(pause HAVE_PAUSE) 
CHECK_FUNCTION_EXISTS(sigsetmask HAVE_SIGSETMASK) 
CHECK_FUNCTION_EXISTS(memcmp HAVE_MEMCMP) 
CHECK_FUNCTION_EXISTS(memcpy HAVE_MEMCPY) 
CHECK_FUNCTION_EXISTS(memset HAVE_MEMSET) 
CHECK_FUNCTION_EXISTS(strchr HAVE_STRCHR) 
CHECK_FUNCTION_EXISTS(strrchr HAVE_STRRCHR) 
CHECK_FUNCTION_EXISTS(random HAVE_RANDOM) 
CHECK_FUNCTION_EXISTS(srandom HAVE_SRANDOM) 
CHECK_FUNCTION_EXISTS(usleep HAVE_USLEEP) 
CHECK_FUNCTION_EXISTS(ioctl HAVE_IOCTL) 
CHECK_FUNCTION_EXISTS(setpgrp HAVE_SETPGRP) 

CHECK_FUNCTION_EXISTS(crypt HAVE_CRYPT)
IF (NOT HAVE_CRYPT)
    CHECK_LIBRARY_EXISTS(crypt crypt "" HAVE_LIBCRYPT)
ENDIF (NOT HAVE_CRYPT)

### Library checks
FIND_PACKAGE (GDBM)
FIND_PACKAGE (RSA)
FIND_PACKAGE (Curses)
FIND_PACKAGE (X11)
FIND_LIBRARY (MATH_LIBRARIES m)


### Executable check
FIND_PROGRAM (UPTIME_EXECUTABLE
    	NAMES uptime
    	PATHS /usr/bin /usr/local/bin /opt/local/bin
    	DOC "uptime executable"
)
FIND_PROGRAM (XBIFF_EXECUTABLE
    	NAMES xbiff
    	PATHS /usr/bin /usr/local/bin /opt/local/bin
    	DOC "xbiff executable"
)

### Defined in include/config.h.in but looks to always be on?

IF (RSA)
	ADD_DEFINITIONS("-DRSA -DSHOW_RSA")
	MESSAGE (STATS "Building with RSA components active")
ENDIF (RSA)

ADD_DEFINITIONS(
	"-DSERVER"				# Required
	"-DSVERS"				# Required
	"-DLTD_STATS"			# Required
	"-DFEATURES"
	"-DFEATURE_PACKETS"
	"-DPING"
	"-DRCD"
	"-DCHECKMESG"
	"-DPRETSERVER"
#    ${ZLIB_DEFINITIONS}
)

INCLUDE_DIRECTORIES (
#    ${ZLIB_INCLUDE_DIR}
)

message(STATUS "Version: ${VERSION}")

CONFIGURE_FILE (${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# do_library do_server rules
ADD_SUBDIRECTORY (ntserv)

# do_listener
ADD_SUBDIRECTORY (newstartd)

# do_robots
ADD_SUBDIRECTORY (robots)

# do_robotsd
ADD_SUBDIRECTORY (robotd)

# do_utilities
ADD_SUBDIRECTORY (tools)
ADD_SUBDIRECTORY (sequencer)

IF (RSA)
	# do_rsa_library
	ADD_SUBDIRECTORY (res-rsa)
ENDIF(RSA)

# do_rsa_utilities
ADD_SUBDIRECTORY (keycomp)

# do_xsg?
ADD_SUBDIRECTORY (xsg)

# do_pledit
ADD_SUBDIRECTORY (pledit)



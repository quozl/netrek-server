### First setup the includes and link paths
INCLUDE_DIRECTORIES (
        ${CMAKE_BINARY_DIR} 
        ${CMAKE_SOURCE_DIR}/include
)

### Equivalent to R_OBJS in Makefile.in
set(ntserv_SOURCES
	cluecheck.c 
	death.c 
	findslot.c 
	getentry.c 
	getname.c 
	input.c 
	main.c 
	redraw.c
	reserved.c
	timecheck.c
	ntscmds.c
	queue.c 
)

### Equivalent to D_OBJS in Makefile.in
SET (daemon_SOURCES
	daemon.c
	queue.c
	wander2.c
	solicit.c 
	conquer.c
)

### Equivalent to R_FILES in Makefile.in
set(common_SOURCES
	cluecheck.c 
	coup.c 
	data.c 
	db.c 
	death.c 
	detonate.c  
	enter.c 
	findslot.c  
	getentry.c 
	getname.c  
	salt.c 
	getship.c 
	input.c 
	interface.c  
	main.c  
	orbit.c 
	phaser.c  
	plasma.c 
	redraw.c  
	reserved.c  
	sintab.c 
	socket.c  
	genspkt.c  
	smessage.c 
	startrobot.c  
	timecheck.c 
	torp.c  
	util.c  
	warning.c 
	ping.c 
	getpath.c 
	distress.c 
	transwarp.c 
	gencmds.c 
	ntscmds.c 
	openmem.c 
	feature.c 
	queue.c 
	slotmaint.c 
	wander2.c 
	sysdefaults.c 
	rsa_key.c
)

### Defined in include/config.h.in but looks to always be on?
#ADD_DEFINITIONS ("-DRSA")

### CFlags
if (WITH_FLAG_WALL)
    add_definitions("-Wall")
endif (WITH_FLAG_WALL)

if (WITH_FLAG_ANSIPEDANTIC)
    add_definitions("-pedantic -ansi")
endif (WITH_FLAG_ANSIPEDANTIC)


### Libraries
if (HAVE_LIBGDBM)
    SET (GDBM_LIBRARIES gdbm)
endif (HAVE_LIBGDBM)

IF (HAVE_LIBCRYPT)
	SET (CRYPT_LIBRARIES crypt)
ENDIF (HAVE_LIBCRYPT)

IF (HAVE_LIBRSA)
	SET (RSA_LIBRARIES rsa-gmp)
ENDIF (HAVE_LIBRSA)

### Sources activated via Defines
if (TRIPLE_PLANET_MAYHEM)
	set(BALANCE balance.c)
endif (TRIPLE_PLANET_MAYHEM)

if (RSA)
	set(RSA_KEY rsa_key.c)
endif (RSA)

if (STURGEON)
	set(STURGEON sturgeon.c)
endif (STURGEON)

### Equivalent to L_OBJS from Makefile.in
ADD_LIBRARY (netrek STATIC 
	advertise.c
	alarm.c 
	${BALANCE}
	bans.c 
	bay.c 
	blog.c 
	coup.c 
	data.c
	db.c 
	detonate.c 
	distress.c 
	draft.c
	enter.c 
	feature.c 
	gencmds.c 
	genspkt.c 
	getpath.c 
	getship.c 
	glog.c 
	interface.c
	ip.c 
	ltd_stats.c
	openmem.c 
	orbit.c 
	phaser.c 
	ping.c 
	planet.c 
	plasma.c 
	${RSA_KEY}
	reserved.c 
	salt.c 
	slotmaint.c
	sigpipe.c 
	sintab.c 
	smessage.c 
	socket.c 
	solicit.c 
	startrobot.c 
	${STURGEON}
	sysdefaults.c 
	torp.c 
	transwarp.c 
	util.c 
	warning.c
)


### ntserv linker instructions
ADD_EXECUTABLE (    
	ntserv 
    ${ntserv_SOURCES}
)

TARGET_LINK_LIBRARIES (
	ntserv
	netrek
	${RSA_LIBRARIES}
	${GDBM_LIBRARIES}
	${CRYPT_LIBRARIES}
	${MATH_LIBRARIES}
)

### daemon linker instructions
ADD_EXECUTABLE (
	daemon
	${daemon_SOURCES}
)

TARGET_LINK_LIBRARIES (
	daemon
	netrek
	${MATH_LIBRARIES}
)

### Install targets
# LIBDIR = lib
# LOCALSTATEDIR = var
INSTALL (
	TARGETS ntserv daemon
	RUNTIME DESTINATION lib
)

### First setup the includes and link paths
INCLUDE_DIRECTORIES (
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}	
)

### Defines
ADD_DEFINITIONS (-DATM)


### Equivalent to R_OBJS in Makefile.in
SET (robot_SOURCES
	robot.c
	data.c
	death.c
	decide.c
	defaults.c
	disengage.c
	dmessage.c
	dodge.c
	engage.c
	enter.c
	escort.c
	findslot.c
	getarmies.c
	getname.c
	getship.c
	main.c
	master.c
	newwin.c
	ogg.c
	parse.c
	ping.c
	reserved.c
	shmem.c
	sintab.c
	socket.c
	assault.c
	util.c
	update_players.c
	update_planets.c
	warfare.c
	pwarfare.c
	rwarfare.c
	ships.c
	input.c
	redraw.c
)

### puck linker instructions
ADD_EXECUTABLE (    
	robot 
    ${robot_SOURCES}
)

TARGET_LINK_LIBRARIES (
	robot
	${MATH_LIBRARIES}
)

### Install targets
INSTALL (
        TARGETS robot
        RUNTIME DESTINATION lib/og
)

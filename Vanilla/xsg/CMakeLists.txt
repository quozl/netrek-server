### First setup the includes and link paths
INCLUDE_DIRECTORIES (
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}	
)

### Defines
ADD_DEFINITIONS ("-DNBR")

SET (xsg_SOURCES
	colors.c 
	defaults.c 
	dmessage.c 
	inform.c 
	input.c 
	main.c 
	modify.c 
	newwin.c 
	option.c
	planetlist.c 
	planets.c 
	playerlist.c 
	redraw.c 
	robotwin.c 
#	shmem.c 
	sintab.c 
	smessage.c 
	stats.c 
	util.c 
	war.c 
	warning.c 
	x11window.c 
	localdata.c
)

### xsg linker
ADD_EXECUTABLE (xsg ${xsg_SOURCES})
TARGET_LINK_LIBRARIES (xsg 
	netrek
	${X11_LIBRARIES}
	${MATH_LIBRARIES}
)

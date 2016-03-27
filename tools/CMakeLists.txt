### First setup the includes and link paths
INCLUDE_DIRECTORIES (
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}	
)

### Defines
#ADD_DEFINITIONS ("-DFEATURE_PACKETS")

### Libraries
IF (HAVE_LIBCRYPT)
	SET (CRYPT_LIBRARIES crypt)
ENDIF (HAVE_LIBCRYPT)

### Equivalent to SRCS in Makefile.in
SET (all_SOURCES
	ban.c 
	getpath.c
	blotpassword.c 
	loadchecker.c
	mess.c
	mergescores.c
	newscores.c 
	planets.c 
	players.c
	scores.c
	setgalaxy.c
	stat.c
	trimscores.c
	watchmes.c
	fun.c 
	xtkill.c
	keyman.c
	nuke.c
	metaget.c
	update.c
	convert.c
	conq_vert.c
	sortdb.c
	ntpasswd.c
	setgame.c
	setplanet.c
	setteam.c
	setship.c
	delist.c
)

SET (TOOLS
	cambot 
	sortdb
	mess
	message
	mergescores
	newscores
	planets
	nuke
	scores
	setgalaxy
	stat
	blotpassword
	trimscores
	watchmes
	xtkill
	keyman
	fun
	convert
	ltd_dump
	ltd_convert
	conq_vert
	metaget
	setgame
	setplanet
	setteam
	setship
	delist
	ban
	getpath
	ntpasswd
)

SET (SCRIPTS
	updatekeys
	blosxom
	blog
	blog-file
	blog-update
	blog-update-all
	blog-purge-old
	cambot-clean
	cambot-cyclic
	ignore-clean
)

SET (ADMIN_SCRIPTS
	admin/ban
	admin/free
	admin/kill
	admin/quit
	admin/reset
)

FOREACH (tool ${TOOLS})
	ADD_EXECUTABLE (${tool} ${tool}.c)
	TARGET_LINK_LIBRARIES (${tool} 
		netrek
		${CRYPT_LIBRARIES}
		${MATH_LIBRARIES}
	)
ENDFOREACH(tool)

### showgalaxy needs a little bit more LIBs and linker help
ADD_EXECUTABLE (showgalaxy showgalaxy.c)
TARGET_LINK_LIBRARIES (showgalaxy 
	netrek
	${CURSES_LIBRARY}
)

### ltd_dump_html needs a little bit more LIBs and linker help
ADD_EXECUTABLE (ltd_dump_html ltd_dump_html.c)
TARGET_LINK_LIBRARIES (ltd_dump_html
	netrek
	${CRYPT_LIBRARIES}
)

### Not sure why it's called updated but build on update. :-(
ADD_EXECUTABLE (updated update.c)
TARGET_LINK_LIBRARIES (updated 
	netrek
)

###	trekon required xbiff
IF (XBIFF_EXECUTABLE)
	MESSAGE (STATUS "Not implemented yet.")
ENDIF (XBIFF_EXECUTABLE)

### Install targets
INSTALL (
	TARGETS ${TOOLS}
	RUNTIME DESTINATION lib/tools
)

INSTALL (
	FILES ${SCRIPTS}
	DESTINATION lib/tools
)

INSTALL (
	FILES ${ADMIN_SCRIPTS}
	DESTINATION lib/tools/admin
)

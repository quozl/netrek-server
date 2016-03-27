### First setup the includes and link paths
INCLUDE_DIRECTORIES (
        ${CMAKE_BINARY_DIR} 
        ${CMAKE_SOURCE_DIR}/include
)

### Defines
ADD_DEFINITIONS ("-DPORT=2592")

### Equivalent to N_OBJS in Makefile.in
SET (newstartd_SOURCES
	newstartd.c
)

### newstartd linker instructions
ADD_EXECUTABLE (    
	newstartd 
    ${newstartd_SOURCES}
)

TARGET_LINK_LIBRARIES (
	newstartd
	netrek
	${MATH_LIBRARIES}
)

### Install targets
INSTALL (
	TARGETS newstartd
	RUNTIME DESTINATION lib
)

### Symlink hacking. Should install it in bin or lib, not both?
# Symlink aren't all that platform independent so I think that is why making
# symlinks in cmake is convoluted (at least how I've figure them out).
#
INSTALL (DIRECTORY DESTINATION bin)
INSTALL (CODE "EXECUTE_PROCESS (
	COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_INSTALL_PREFIX}/lib/newstartd ${CMAKE_INSTALL_PREFIX}/bin/netrekd
    )"
)

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

### First setup the includes and link paths
INCLUDE_DIRECTORIES (
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}	
)

### Defines
#ADD_DEFINITIONS ("-DFEATURE_PACKETS")

### Whack-a-do for making ntserv/commands.c into squencer/commands_sequencer.o
set(COMMANDS_TYPE SEQUENCER)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/commands.c.cmake
	${CMAKE_CURRENT_BINARY_DIR}/commands_sequencer.c
)

### Equivalent to S_OBJS in Makefile.in
SET (sequencer_SOURCES
	sequencer.c 
	${CMAKE_CURRENT_BINARY_DIR}/commands_sequencer.c
)

### sequencer
ADD_EXECUTABLE (sequencer ${sequencer_SOURCES})
TARGET_LINK_LIBRARIES (sequencer netrek)

### Install targets
INSTALL (
	TARGETS sequencer
        RUNTIME DESTINATION lib
)

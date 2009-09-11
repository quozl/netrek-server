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

SET (pledit_SOURCES
	main.c 
	edit.c 
	input.c
)

### pledit linker
ADD_EXECUTABLE (pledit ${pledit_SOURCES})
TARGET_LINK_LIBRARIES (pledit 
	netrek 
	${CRYPT_LIBRARIES}
	${CURSES_LIBRARY}
)

### Install targets
INSTALL (
	TARGETS pledit
	RUNTIME DESTINATION lib
)

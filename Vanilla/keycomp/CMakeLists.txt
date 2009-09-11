### First setup the includes and link paths
INCLUDE_DIRECTORIES (
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}	
)

### Defines
#ADD_DEFINITIONS ("-DFEATURE_PACKETS")

SET (TOOLS
	rsa_keycomp
	rsa_key2cap
)

FOREACH (tool ${TOOLS})
	ADD_EXECUTABLE (${tool} ${tool}.c)
	TARGET_LINK_LIBRARIES (${tool} netrek)
ENDFOREACH(tool)

### Install targets
INSTALL (
	TARGETS ${TOOLS}
        RUNTIME DESTINATION lib
)

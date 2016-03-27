MACRO (COPY_IF_NOT_EXISTS _SRC _DEST)
	SET (SRC "${_SRC}")
	SET (DEST "${_DEST}")
	#MESSAGE ("${SRC} to ${DEST}")
	
	IF (NOT EXISTS ${DEST})
		EXECUTE_PROCESS (COMMAND ${CMAKE_COMMAND} -E copy ${_SRC}
                                                          ${DEST})
	ENDIF (NOT EXISTS ${DEST})
ENDMACRO (COPY_IF_NOT_EXISTS _SRC _DEST)

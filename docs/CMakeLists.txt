MACRO (COPY_IF_NOT_EXISTS _SRC _DEST)
	SET (SRC "${CMAKE_CURRENT_SOURCE_DIR}/${_SRC}")
	SET (DEST "${CMAKE_INSTALL_PREFIX}/${_DEST}")
	MESSAGE ("Copy ${SRC} to ${DEST}")

	IF (NOT EXISTS ${DEST})
		EXECUTE_PROCESS (COMMAND ${CMAKE_COMMAND} -E copy ${_SRC}
                                                          ${DEST})
	ENDIF (NOT EXISTS ${DEST})
ENDMACRO (COPY_IF_NOT_EXISTS _SRC _DEST)

### Setup parameterized cmake script
CONFIGURE_FILE (
	${CMAKE_CURRENT_SOURCE_DIR}/CopyIfNotExists.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/CopyIfNotExists.cmake
)

CONFIGURE_FILE (
	${CMAKE_CURRENT_SOURCE_DIR}/sample_sysdef.in
	${CMAKE_CURRENT_BINARY_DIR}/sysdef
)


### Equivalent to DOCS in Makefile.in
SET (all_DOCS
	sample_features 
	sample_motdclue 
	sample_sysdef 
	sample_motd 
	sample_ports 
	sample_time 
	sample_motdbasep 
	sample_reserved 
	sample_tourn.map
)

set (empty_ETC_DIRS
	ip/duplicates
	ip/whitelist
	ip/hide
	ip/mute
	ip/muteall
	ip/ignore
	ip/deny
)

set (empty_VAR_DIRS
	ip/ignore/doosh
	ip/ignore/multi
	ip/ignore/by-ip
)

set (empty_FILES
	nocount
	banned
	bypass
	clue-bypass
)

INSTALL (SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/CopyIfNotExists.cmake)

FOREACH (dir ${empty_ETC_DIRS})
	INSTALL (DIRECTORY DESTINATION etc/${dir})
ENDFOREACH (dir)

FOREACH (dir ${empty_VAR_DIRS})
	INSTALL (DIRECTORY DESTINATION var/${dir})
ENDFOREACH (dir)

FOREACH (empty ${empty_FILES})
	EXECUTE_PROCESS (COMMAND ${CMAKE_COMMAND} -E touch ${empty})
  	INSTALL (FILES ${CMAKE_BINARY_DIR}/${empty} DESTINATION etc)
ENDFOREACH (empty)

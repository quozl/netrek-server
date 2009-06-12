# - Find rsa
# Find the native RSA includes and library
#
#  RSA_INCLUDE_DIR - where to find rsa.h, etc.
#  RSA_LIBRARIES   - List of libraries when using rsa.
#  RSA_FOUND       - True if rsa found.

FIND_PACKAGE (GMP)

IF (RSA_INCLUDE_DIR)
  # Already in cache, be silent
  SET(RSA_FIND_QUIETLY TRUE)
ENDIF (RSA_INCLUDE_DIR)

FIND_PATH(RSA_INCLUDE_DIR rsa_gmp.h
   # Look in system locations
  /usr/local/include
  /usr/include
  /opt/local/include
  # Look in source tree and out-of-source tree
  ${CMAKE_SOURCE_DIR}/res-rsa
  ${CMAKE_SOURCE_DIR}/res-rsa/build
)

SET(RSA_NAMES rsa-gmp)
FIND_LIBRARY(RSA_LIBRARY
  NAMES ${RSA_NAMES}
  PATHS /usr/lib /usr/local/lib /opt/local/lib ${CMAKE_SOURCE_DIR}/res-rsa ${CMAKE_SOURCE_DIR}/res-rsa/build
)

# Did we find RSA in system locations?
IF (RSA_INCLUDE_DIR AND RSA_LIBRARY)
	SET (RSA_FOUND TRUE)
	SET (RSA_LIBRARIES ${RSA_LIBRARY})	
ELSE (RSA_INCLUDE_DIR AND RSA_LIBRARY)
   SET (RSA_FOUND FALSE)
   SET (RSA_LIBRARIES)
ENDIF (RSA_INCLUDE_DIR AND RSA_LIBRARY)

# Check in Vanilla's source tree for RSA
IF (RSA_INCLUDE_DIR AND NOT RSA_FOUND)
	SET (RSA_LIBRARY ${RSA_NAMES})
	SET (RSA_LIBRARIES ${RSA_LIBRARY})
ENDIF (RSA_INCLUDE_DIR AND NOT RSA_FOUND)

IF (GMP_FOUND AND RSA_FOUND)
	SET (RSA_INCLUDE_DIR ${RSA_INCLUDE_DIR} ${GMP_INCLUDE_DIR})
	SET (RSA_LIBRARIES ${RSA_LIBRARIES} ${GMP_LIBRARIES})
ENDIF (GMP_FOUND AND RSA_FOUND)

IF (RSA_FOUND)
   IF (NOT RSA_FIND_QUIETLY)
      MESSAGE(STATUS "Found RSA: ${RSA_LIBRARY}")
   ENDIF (NOT RSA_FIND_QUIETLY)
ELSE (RSA_FOUND)
   IF (RSA_FIND_REQUIRED)
      MESSAGE(STATUS "Looked for rsa libraries named ${RSAS_NAMES}.")
      MESSAGE(FATAL_ERROR "Could NOT find rsa library")
   ENDIF (RSA_FIND_REQUIRED)
ENDIF (RSA_FOUND)

MARK_AS_ADVANCED(
  RSA_LIBRARY
  RSA_LIBRARIES
  RSA_INCLUDE_DIR
)

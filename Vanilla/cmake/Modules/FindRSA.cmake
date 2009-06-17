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

# Is GMP available?
IF (GMP_FOUND)
	SET (RSA_LIB_NAME rsa-gmp)
	SET (RSA_H_NAME rsa_gmp.h)
ELSE (GMP_FOUND)
	SET (RSA_LIB_NAME rsa)
	SET (RSA_H_NAME rsa.h)
ENDIF (GMP_FOUND)

FIND_PATH(RSA_INCLUDE_DIR ${RSA_H_NAME}
   # Look in system locations
  /usr/local/include
  /usr/include
  /opt/local/include
  # Look in source tree and out-of-source tree
  ${CMAKE_SOURCE_DIR}/res-rsa
  ${CMAKE_SOURCE_DIR}/res-rsa/build
)

FIND_LIBRARY(RSA_LIBRARY
  NAMES ${RSA_LIB_NAME}
  PATHS /usr/lib /usr/local/lib /opt/local/lib ${CMAKE_SOURCE_DIR}/res-rsa ${CMAKE_SOURCE_DIR}/res-rsa/build
)

# Did we find RSA in system locations?
IF (RSA_INCLUDE_DIR AND RSA_LIBRARY)
	SET (RSA_FOUND TRUE)
	SET (RSA_LIBRARIES ${RSA_LIB_NAME})	
ENDIF (RSA_INCLUDE_DIR AND RSA_LIBRARY)

# Did we find RSA include file but -not- the library?
IF (RSA_INCLUDE_DIR AND NOT RSA_LIBRARY)
   SET (RSA_FOUND FALSE)
   SET (RSA_LIBRARIES)
ENDIF (RSA_INCLUDE_DIR AND NOT RSA_LIBRARY)

# Check in Vanilla's source tree for RSA
IF (RSA_INCLUDE_DIR AND NOT RSA_FOUND)
	SET (RSA_LIBRARY ${RSA_LIB_NAME})
	SET (RSA_LIBRARIES ${RSA_LIB_NAME})
	MESSAGE (STATUS "RSA found in-source")
ENDIF (RSA_INCLUDE_DIR AND NOT RSA_FOUND)

# If GMP add gmp's include and libraries
IF (GMP_FOUND)
	SET (RSA_INCLUDE_DIR ${RSA_INCLUDE_DIR} ${GMP_INCLUDE_DIR})
	SET (RSA_LIBRARIES ${RSA_LIBRARIES} ${GMP_LIBRARIES})
ENDIF (GMP_FOUND)

MESSAGE(STATUS "XXXX: ${RSA_LIBRARIES}")

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

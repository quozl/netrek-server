### First setup the includes and link paths
INCLUDE_DIRECTORIES (
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_CURRENT_SOURCE_DIR}	
)

### Defines
ADD_DEFINITIONS ("-DROBOT")

### Whack-a-do for making ntserv/commands.c into robots/commands_puck.o
set(COMMANDS_TYPE PUCK)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/commands.c.cmake
	${CMAKE_CURRENT_BINARY_DIR}/commands_puck.c
)

### Equivalent to PUCK_OBJS in Makefile.in
SET (puck_SOURCES
	puck.c
	puckmove.c
	${CMAKE_CURRENT_BINARY_DIR}/commands_puck.c
	roboshar.c
)

### puck linker instructions
ADD_EXECUTABLE (    
	puck 
    ${puck_SOURCES}
)

TARGET_LINK_LIBRARIES (
	puck
	netrek
	${MATH_LIBRARIES}
)


### Whack-a-do for making ntserv/commands.c into robots/commands_mars.o
set(COMMANDS_TYPE DOG)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/commands.c.cmake
	${CMAKE_CURRENT_BINARY_DIR}/commands_mars.c
)

### Equivalent to M_OBJS in Makefile.in
SET (mars_SOURCES
	mars.c
	marsmove.c
	${CMAKE_CURRENT_BINARY_DIR}/commands_mars.c
	roboshar.c
)

### mars linker instructions
ADD_EXECUTABLE (    
	mars 
    ${mars_SOURCES}
)

TARGET_LINK_LIBRARIES (
	mars
	netrek
	${MATH_LIBRARIES}
)



### Whack-a-do for making ntserv/commands.c into robots/commands.o
set(COMMANDS_TYPE ROBOTII)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/commands.c.cmake
	${CMAKE_CURRENT_BINARY_DIR}/commands.c
)

### R_OBJS
SET (robotII_SOURCES
	robotII.c
	rmove.c
	${CMAKE_CURRENT_BINARY_DIR}/commands.c
	roboshar.c
)

### robotII linker instructions
ADD_EXECUTABLE (    
	robotII 
    ${robotII_SOURCES}
)

TARGET_LINK_LIBRARIES (
	robotII
	netrek
	${MATH_LIBRARIES}
)

### Whack-a-do for making ntserv/commands.c into robots/commands_basep.o
set(COMMANDS_TYPE BASEP)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/commands.c.cmake
	${CMAKE_CURRENT_BINARY_DIR}/commands_basep.c
)

### B_OBJS
SET (basep_SOURCES
	basep.c
	${CMAKE_CURRENT_BINARY_DIR}/commands_basep.c
	roboshar.c
)

### basep linker instructions
ADD_EXECUTABLE (    
	basep 
    ${basep_SOURCES}
)

TARGET_LINK_LIBRARIES (
	basep
	netrek
	${MATH_LIBRARIES}
)

### Whack-a-do for making ntserv/commands.c into robots/commands_newbie.o
set(COMMANDS_TYPE NEWBIE)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/commands.c.cmake
	${CMAKE_CURRENT_BINARY_DIR}/commands_newbie.c
)

### N_OBJS
SET (newbie_SOURCES
	newbie.c
	${CMAKE_CURRENT_BINARY_DIR}/commands_newbie.c
	roboshar.c
)

### newbie linker instructions
ADD_EXECUTABLE (    
	newbie 
    ${newbie_SOURCES}
)

TARGET_LINK_LIBRARIES (
	newbie
	netrek
	${MATH_LIBRARIES}
)

### Whack-a-do for making ntserv/commands.c into robots/commands_pret.o
set(COMMANDS_TYPE PRET)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/commands.c.cmake
	${CMAKE_CURRENT_BINARY_DIR}/commands_pret.c
)

### P_OBJS
SET (pret_SOURCES
	pret.c
	${CMAKE_CURRENT_BINARY_DIR}/commands_pret.c
	roboshar.c
)

### pret linker instructions
ADD_EXECUTABLE (    
	pret 
    ${pret_SOURCES}
)

TARGET_LINK_LIBRARIES (
	pret
	netrek
	${MATH_LIBRARIES}
)

### I_OBJS
SET (inl_SOURCES
	inl.c
	inlcomm.c
	inlcmds.c
	roboshar.c
)

### inl linker instructions
ADD_EXECUTABLE (    
	inl 
	${inl_SOURCES}
)

TARGET_LINK_LIBRARIES (
	inl
	netrek
	${MATH_LIBRARIES}
)

### Install targets
INSTALL (
	TARGETS puck mars robotII basep newbie pret inl
	RUNTIME DESTINATION lib
)

INSTALL (
	FILES end_tourney.pl auto-archive.pl
	DESTINATION lib
)

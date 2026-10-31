/* basepdefs.h

   This file contains many of the defines needed for Base practice
   and not for anything else.

*/
#ifndef _h_basepdefs
#define _h_basepdefs


#include "defs.h"

/* System dependend setups */

#ifdef CHECK_ENV
#define ROBODIR(RFILE) ( strdup(strcat(strcpy(robofile, Robodir), RFILE)))
#else
#define ROBODIR(RFILE) (LIBDIR "/" N_ROBODIR RFILE)
#endif
#ifdef CHECK_ENV
#define ROBOCONFDIR(RFILE) (strdup(strcat(strcpy(robofile, Robodir), RFILE)))
#else
#define ROBOCONFDIR(RFILE) (SYSCONFDIR "/" N_ROBODIR RFILE)
#endif

#define RCMD            ""
#define OROBOT          ROBODIR("/robot")
#define NICE            1
#define REMOTEHOST      ""
#define LOGFILE		ROBODIR("")
#define COMFILE 	ROBOCONFDIR("/og")
#define DOGFILE 	ROBOCONFDIR("/dog")
#define BASEFILE        ROBOCONFDIR("/base")
#define DEFFILE		ROBOCONFDIR("/df")

/* Basepractice specific additions */

#define PORT "2592"

#define HOWOFTEN 1                       /*Robot moves every HOWOFTEN cycles*/
#define PERSEC (1000000/UPDATE/HOWOFTEN) /* # of robo calls per second*/

#define ROBOCHECK (300*PERSEC)		/* Check if only robots are in game */
#define SENDINFO  (600*PERSEC)		/* send info to all */

#define NB_ROBOTS 16

#endif /* _h_basepdefs */


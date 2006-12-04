/* newbiedefs.h

   This file contains many of the defines needed for Newbie Server
   and not for anything else.

*/
#ifndef _h_newbiedefs
#define _h_newbiedefs


#include "defs.h"

/* System dependend setups */

#ifdef CHECK_ENV
#define ROBODIR(RFILE) ( strdup(strcat(strcpy(robofile, Robodir), RFILE)))
#else
#define ROBODIR(RFILE) (LIBDIR "/" N_ROBODIR RFILE)
#endif

#define RCMD            ""
#define OROBOT          ROBODIR("/robot")
#define NICE            ""
#define REMOTEHOST      ""
/* #define TREKSERVER      "newbie.my.domain" */
#define LOGFILE		ROBODIR("")
#define COMFILE 	ROBODIR("/og")
#define DOGFILE 	ROBODIR("/dog")
#define BASEFILE        ROBODIR("/base")
#define DEFFILE		ROBODIR("/df")



/* Newbie server specific additions */

#define PORT "3592"

#define HOWOFTEN 1                       /*Robot moves every HOWOFTEN cycles*/
#define PERSEC (1000000/UPDATE/HOWOFTEN) /* # of robo calls per second*/

#define ROBOCHECK (1*PERSEC)		/* start or stop a robot */
#define ROBOEXITWAIT (5*ROBOCHECK)  /* wait between exiting bots so that
                                       multiple bots don't exit */
#define SENDINFO  (600*PERSEC)		/* send info to all */
#define NEWBIEMSG 1			/* what info msg to send, 0 = off */
#define QUPLAY(A) (queues[A].max_slots - queues[A].free_slots)

#endif /* _h_newbiedefs */


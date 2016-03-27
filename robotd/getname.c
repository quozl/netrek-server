/*
 * getname.c
 * 
 * Kevin P. Smith 09/28/88
 * 
 */
#include "copyright2.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

renter(pseudo, pss, log)
   char           *pseudo, *pss, *log;
{
   int		   guest = (strcmp(pseudo, "guest") == 0);

   loginAccept = -1;

   if(guest){
      sendLoginReq(pseudo, "", log, 0);
      me->p_stats.st_tticks = 1;
      strcpy(me->p_name, pseudo);
   }
   else
      sendLoginReq(pseudo, "", log, 1);

   while (loginAccept == -1) {
      socketPause();
#ifdef ATM
      readFromServer(0);
#else
      readFromServer();
#endif
      if (isServerDead()) {
	 mprintf("Server is dead!\n");
	 exit(0);
      }
   }

   if(!guest){
      if(loginAccept == 0){
	 /* we have to make the password */
	 sendLoginReq(pseudo, pss, log, 0);
	 while (loginAccept == -1) {
	    socketPause();
#ifdef ATM
	    readFromServer(0);
#else
	    readFromServer();
#endif
	    if (isServerDead()) {
	       mprintf("Server is dead!\n");
	       exit(0);
	    }
	 }
      }
      loginAccept = -1;
      sendLoginReq(pseudo, pss, log, 0);
      while (loginAccept == -1) {
	 socketPause();
#ifdef ATM
	 readFromServer(0);
#else
	 readFromServer();
#endif
	 if (isServerDead()) {
	    mprintf("Server is dead!\n");
	    exit(0);
	 }
      }
      if (loginAccept == 0) {
	 fprintf(stderr, "%s: bad password.\n", pseudo);
	 exitRobot(0);
      }
   }
   strcpy(me->p_name, pseudo);
   showShields = (me->p_stats.st_flags / ST_SHOWSHIELDS) & 1;
   mapmode = (me->p_stats.st_flags / ST_MAPMODE) & 1;
   namemode = (me->p_stats.st_flags / ST_NAMEMODE) & 1;
   keeppeace = (me->p_stats.st_flags / ST_KEEPPEACE) & 1;
   showlocal = (me->p_stats.st_flags / ST_SHOWLOCAL) & 3;
   showgalactic = (me->p_stats.st_flags / ST_SHOWGLOBAL) & 3;
}

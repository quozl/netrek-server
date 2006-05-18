/* $Id: input.c,v 1.1 2005/03/21 05:23:43 jerub Exp $
 */

#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <sys/param.h>
#ifdef SCO
#include <sys/itimer.h>
#else
#include <sys/time.h>
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "defs.h"
#include INC_SYS_SELECT
#include "struct.h"
#include "data.h"
#include "proto.h"

#define GHOSTTIME       (30 * 1000000 / UPDATE) /* 30 secs */

static int sendflag=0;
static int do_update =1;
extern int living;

static void setflag()
{
  if (!living) return;
  HANDLE_SIG(SIGALRM,setflag);

  if (do_update)
    intrupt();
  else 	
    sendflag=1;

  if (me->p_updates > delay) {
    me->p_flags &= ~(PFWAR);
  }
  if (me->p_updates > rdelay) {
    me->p_flags &= ~(PFREFITTING);
  }
#ifdef PING
  /* this will ghostbust a player if the number of missing pings from the
     client exceeds the ghostbust interval */
  if (ping_allow_ghostbust && me->p_status == PALIVE && ping_ghostbust >
      ping_ghostbust_interval) {
    /* ghostbusted */
    me->p_ghostbuster = GHOSTTIME;
    ping_ghostbust = 0;
    ERROR(2,("%s: ghostbusted by ping loss\n", 
	     me->p_mapchars));
    fflush(stderr);
  }
#endif /*PING*/
}

static int resurrect(void)
{
  register int i;

  SIGNAL(SIGALRM, SIG_IGN);
#ifndef SCO /* SCO doesn't have SIGIO, so why bother ignoring it? */
  SIGNAL(SIGIO, SIG_IGN);
#endif

  if (!testtime) {
    ERROR(2,("%s: input() commences resurrection\n", me->p_mapchars));
  }

  commMode = COMM_TCP;
  if (udpSock >= 0)
    closeUdpConn();

  /* for next thirty seconds, we try to restore connection */
  shutdown(sock, 2);
  sock= -1;
  for (i=0;; i++) {
    me->p_ghostbuster = 0;
    if (testtime == 1)
      sleep(1);
    else
      sleep(5);
    if (connectToClient(host, nextSocket)) break;
    if (((i>=5) && testtime) || 
	((i>=6) && (!testtime))) {
      ERROR(2,("%s: input() giving up\n", me->p_mapchars));
      freeslot(me);
      exit(0);
    }
  }
  if (testtime) {
    me->p_whydead = KQUIT;
    me->p_status = PEXPLODE;
    ERROR(3,("%s: user attempted to used old reserved.c\n", me->p_mapchars));
    new_warning(UNDEF,"Only RSA clients are valid. Please use a blessed one.");
  }
  else {
    ERROR(2,("%s: resurrected\n", me->p_mapchars));
    testtime = -1;	/* Do verification after GB  - NBT */
  }
  SIGNAL(SIGALRM, setflag);
  return 1;
}

void input(void)
{
    fd_set readfds;
    static struct timeval    poll = {2, 0};	

    SIGNAL(SIGALRM, setflag);

    /* Idea:  read from client often, send to client not so often */
    while (living) {
	if (isClientDead()) {
	    if (noressurect){
		freeslot(me);
		exit(0);
	    }
	    resurrect();
	}
	if (! (status -> gameup & GU_GAMEOK)){
	    freeslot(me);
	    exit(0);
	}
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        if (udpSock >= 0)
                FD_SET(udpSock, &readfds);
	poll.tv_sec=2;
	poll.tv_usec=0;
	if (select(32, &readfds, 0, 0, &poll) > 0) {
	    do_update = 0;
#ifdef PING                     /* reset this here also */
            ping_ghostbust = 0;
#endif

	    if (me->p_updates > delay) {
	        me->p_flags &= ~(PFWAR);
	    }
	    if (me->p_updates > rdelay) {
	        me->p_flags &= ~(PFREFITTING);
	    }

	    readFromClient();
	    me->p_ghostbuster = 0;
	    if (sendflag) {
	      intrupt();
	      sendflag=0;
	    }
	    do_update = 1;
	}

    }
}

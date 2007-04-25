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
#include "sigpipe.h"
#include "packets.h"
#include "slotmaint.h"

#define GHOSTTIME       (30 * 1000000 / UPDATE) /* 30 secs */

static int sendflag=0;
static int do_update =1;
extern int living;

static void setflag()
{
  if (!living) return;

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

  sigpipe_suspend(SIGALRM);
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
  sigpipe_resume(SIGALRM);
  return 1;
}

static void gamedown()
{
  struct badversion_spacket packet;
  packet.type = SP_BADVERSION;
  packet.why = 6;
  sendClientPacket(&packet);
  flushSockBuf();
}

static void panic()
{
  gamedown();
  freeslot(me);
  exit(0);
}

void input(void)
{
    fd_set readfds;
    static struct timeval    poll = {2, 0};
    int rv, afd, nfds;

    sigpipe_create();
    sigpipe_assign(SIGALRM);
    afd = sigpipe_fd();

    /* Idea:  read from client often, send to client not so often */
    while (living) {
	if (isClientDead()) {
	    if (noressurect){
		freeslot(me);
		exit(0);
	    }
	    resurrect();
	}
	if (!(status -> gameup & GU_GAMEOK)) panic();
	/* wait for activity on network socket or next daemon update */
	while (1) {
            FD_ZERO(&readfds);
            nfds = 0;
            FD_SET(afd, &readfds);
            if (nfds < afd) nfds = afd;
            FD_SET(sock, &readfds);
            if (nfds < sock) nfds = sock;
            if (udpSock >= 0) {
                FD_SET(udpSock, &readfds);
                if (nfds < udpSock) nfds = udpSock;
	    }
	    poll.tv_sec = 5;
	    poll.tv_usec = 0;
	    rv = select(nfds+1, &readfds, 0, 0, &poll);
	    if (rv > 0) break;
	    if (rv == 0) { panic(); /* daemon silence timeout */ }
	    if (errno == EINTR) continue;
	    perror("select");
	    panic();
	}
	/* if daemon signalled us, perform the update only */
	if (FD_ISSET(afd, &readfds)) {
	    if (sigpipe_read() == SIGALRM) setflag();
	    continue;
	}
	/* otherwise handle input from client */
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
    sigpipe_suspend(SIGALRM);
    sigpipe_close();
}

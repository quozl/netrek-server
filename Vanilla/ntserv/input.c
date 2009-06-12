/* $Id: input.c,v 1.1 2005/03/21 05:23:43 jerub Exp $
 */

#include "copyright.h"
#include "config.h"

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
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "defs.h"
#include <sys/select.h>
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "sigpipe.h"
#include "packets.h"
#include "slotmaint.h"

#define GHOSTTIME       (ghostbust_timer * 1000000 / UPDATE) /* 30 secs */

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
  /* ghostbust a player if the number of missing pings from the
     client exceeds the ghostbust interval */
  if (ping_allow_ghostbust && me->p_status == PALIVE && ping_ghostbust >
      ping_ghostbust_interval) {
    /* ghostbusted */
    me->p_ghostbuster = GHOSTTIME;
    ping_ghostbust = 0;
    ERROR(2,("%s: ghostbusted by ping loss\n", me->p_mapchars));
    fflush(stderr);
  }
}

static void gamedown(why)
{
  struct badversion_spacket packet;
  memset(&packet, 0, sizeof(struct badversion_spacket));
  packet.type = SP_BADVERSION;
  packet.why = why;
  sendClientPacket(&packet);
  flushSockBuf();
}

static void panic(int why)
{
  gamedown(why);
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
            freeslot(me);
            exit(0);
	}
	if (!(status -> gameup & GU_GAMEOK)) panic(BADVERSION_DOWN);
	if (me->p_disconnect) panic(me->p_disconnect);
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
	    if (rv == 0) { panic(BADVERSION_SILENCE); /* daemon timeout */ }
	    if (errno == EINTR) continue;
	    perror("select");
	    panic(BADVERSION_SELECT);
	}
	/* if daemon signalled us, perform the update only */
	if (FD_ISSET(afd, &readfds)) {
	    if (sigpipe_read() == SIGALRM) setflag();
	    continue;
	}
	/* otherwise handle input from client */
	do_update = 0;
	ping_ghostbust = 0;

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

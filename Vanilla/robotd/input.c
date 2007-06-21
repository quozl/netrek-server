/*
 * input.c
 *
 * Modified to work as client in socket based protocol
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#ifdef hpux
#include <time.h>
#else
#include <sys/time.h>
#endif
#include <signal.h>
#include <errno.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"

int	recflag = 0;


/* setflag()
 *
 * Returns an incrementing number from 0 during start of program
 * and stores it in global variable _udcounter. 
 * _udcounter increments by 1 for every 100ms of time that 
 * has passed using the mtime function, which returns 
 * gettimeofday information to the nearest millisecond.
 *
 * Function used to return negative numbers when mtime
 * cycled (anywhere between 2 and 18 hours). Added code 
 * so udcounter is always incrementing
 *
 * Function may still break if unix super-user executes 
 * set-time-of-day function, via unix command. 
 */ 
setflag()
{
   static int start=0; /* start of the robot program */
   static int cycle=0; /* how many times udtime cycles */
   static int cyclestarted=0;
   int udtime; /* 100ms increments JKH */
   
   udtime = mtime(1)/100;

   if(!start){
      start = udtime;
      _udcounter = 0;
   }
   else {
      _udcounter = udtime - start + 
         (cycle * ( (0x0000ffff*1000+999)/100 + 1 ) );
   }                  /* the max udtime could ever be + 1 */

   /* increment cycle once when udtime flips */
   if ( (udtime < start) && (cyclestarted == 0) ) {
      cyclestarted = 1;
      cycle = cycle + 1;
   }

   /* reset cyclestart when udtime is "normal" again */
   if ( udtime > start ) {
      cyclestarted = 0;
   }

}

input()
{
#ifdef nodef
   struct itimerval	udt;
   udt.it_interval.tv_sec = 0;
   udt.it_interval.tv_usec = 100000;
   udt.it_value.tv_sec = 0;
   udt.it_value.tv_usec = 100000;
   setitimer(ITIMER_REAL, &udt, 0);
   signal(SIGALRM, setflag);
#endif

   while (1) {

      setflag();
      intrupt();

      if (isServerDead()) {
	 printf("Whoops!  We've been ghostbusted!\n");
	 printf("Pray for a miracle!\n");
#ifdef ATM
	 /* UDP fail-safe */
	 commMode = commModeReq = COMM_TCP;
	 commSwitchTimeout = 0;
	 if (udpSock >= 0)
	    closeUdpConn();
#endif

	 connectToServer(nextSocket);
	 printf("Yea!  We've been resurrected!\n");
      }
      R_NextCommand();
      if(read_stdin)
	 process_stdin();
   }
}

static char	_buf[80];

process_stdin()
{
   struct timeval	timeout;
   fd_set		readfd;
   int			v;

   timeout.tv_sec = 0;
   timeout.tv_usec = 0;
   FD_ZERO(&readfd);
   FD_SET(0, &readfd);
   v = select(32, &readfd, NULL, NULL, &timeout);
   if(v > 0){
      if(fgets(_buf, 80, stdin) != NULL)
	 R_ProcMessage(_buf, 0, -1, (unsigned char)-1, (unsigned char)1,0);
   }
   else if(v < 0){
      perror("process_stdin'select");
   }
}

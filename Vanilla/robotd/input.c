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
#else hpux
#include <sys/time.h>
#endif hpux
#include <signal.h>
#include <errno.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"

int	recflag = 0;

setflag()
{
   static int start;
   if(!start){
      start = mtime(1)/100;
      _udcounter = 0;
   }
   else
      _udcounter = mtime(1)/100 - start;
   /*
   printf("_udcounter %d\n", _udcounter);
   */
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
#endif nodef

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
#endif ATM

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
	 R_ProcMessage(_buf, 0, -1, (unsigned char)-1, (unsigned char)1);
   }
   else if(v < 0){
      perror("process_stdin'select");
   }
}

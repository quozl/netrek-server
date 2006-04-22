#include <stdio.h>
#include <sys/types.h>
#ifdef mips
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <limits.h>
#include <math.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

master()
{}

/* slave robot */

connect_master(m)

   char	*m;
{
   int			v, ms = -1;
   int			port;
   char			host[80];
   struct sockaddr_in	addr;
   struct hostent	*hp;

   v = sscanf(m, "%*s %s %d", host, &port);
   if(v != 2) {
      fprintf(stderr, "connect_master: bad string %s\n", m);
      return 0;
   }

   if(_master_sock != -1){
      shutdown(_master_sock, 2);
      _master_sock = -1;
   }
   if((ms = socket(AF_INET, SOCK_STREAM, 0)) < 0){
      perror("socket");
      return 0;
   }
   addr.sin_family = AF_INET;
   addr.sin_port   = port;

   if((addr.sin_addr.s_addr = inet_addr(host)) == -1){
      if((hp = gethostbyname(host))==NULL){
	 perror("unknown host");
	 close(ms);
	 return 0;
      }
      else{
	 addr.sin_addr.s_addr = *(long *) hp->h_addr;
      }
   }

   if(connect(ms, (struct sockaddr *) &addr, sizeof(addr)) < 0){
      perror("connect");
      close(ms);
      return 0;
   }
   _master_sock = ms;
   return 1;
}

/* slave sends master packets */

send_to_master(p, s)
   
   struct player_spacket	*p;
   int				s;
{
   int	c;
   if(_master_sock == -1) return;

   c = write(_master_sock, (char *)p, s);
   if(c < 0){
      perror("write");
   }
}

recv_from_master(p)

   struct mastercomm_spacket	*p;
{
}

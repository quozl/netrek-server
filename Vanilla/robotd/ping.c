/*
 * ping.c
 *
 */

#include "copyright2.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include <errno.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"

static int	ping_iloss_sc=0; /* inc % loss 0--100, server to client */
static int	ping_iloss_cs=0; /* inc % loss 0--100, client to server */
static int	ping_tloss_sc=0; /* total % loss 0--100, server to client */
static int	ping_tloss_cs=0; /* total % loss 0--100, client to server */
static int	ping_lag=0;     /* delay in ms of last ping */
static int	ping_av=0;	/* rt time */
static int	ping_sd=0;	/* std deviation */

static int	sum, n, s2;
static int	M, var;

handlePing(packet)                         /* SP_PING */
   struct ping_spacket *packet;
{ 
   ping = 1;		/* we got a ping */

/*
printf("ping received at %d (lag: %d)\n", msetime(), (int)packet->lag);
*/
   sendServerPingResponse((int)packet->number); 
   ping_lag		= ntohs(packet->lag);
   ping_iloss_sc	= (int) packet->iloss_sc;
   ping_iloss_cs	= (int) packet->iloss_cs;
   ping_tloss_sc	= (int) packet->tloss_sc;
   ping_tloss_cs	= (int) packet->tloss_cs;

   calc_lag();
}

startPing()
{
   static
   struct ping_cpacket 	packet;
   extern int		serverDead;

   packet.type = CP_PING_RESPONSE;
   packet.pingme = 1;

   if(gwrite(sock, (char *)&packet, sizeof(struct ping_cpacket))!=
      sizeof(struct ping_cpacket)){
      mprintf("gwrite failed.\n");
      serverDead = 1;
   }
}

stopPing()
{
   static
   struct ping_cpacket 	packet;
   extern int		serverDead;

   packet.type = CP_PING_RESPONSE;
   packet.pingme = 0;

   if(gwrite(sock, (char *)&packet, sizeof(struct ping_cpacket))!=
      sizeof(struct ping_cpacket)){
      mprintf("gwrite failed.\n");
      serverDead = 1;
   }
}
   

sendServerPingResponse(number)     	/* CP_PING_RESPONSE */
   int number;
{
   struct ping_cpacket 	packet;
   int			s;
   extern int		serverDead;

   if(udpSock >= 0){
      s = udpSock;
      packets_sent ++;
   }
   else
      s = sock;

   packet.type                  = CP_PING_RESPONSE;
   packet.pingme		= (char) ping;
   packet.number                = (unsigned char) number;
				  /* count this one */
   packet.cp_sent     		= htonl(packets_sent);
   packet.cp_recv     		= htonl(packets_received);

/*
printf("ping response sent at %d\n", msetime());
*/

   if(gwrite(s, (char *)&packet, sizeof(struct ping_cpacket))!=
      sizeof(struct ping_cpacket)){
      printf("gwrite failed.\n");
      serverDead = 1;
   }
}

calc_lag()
{
#ifdef nodef
   /* probably ghostbusted */
   if(ping_lag > 2000 || ping_lag == 0)
      return;
#endif

   n++;
   sum += ping_lag;
   s2 += (ping_lag * ping_lag);
   if(n == 1) return;

   M = sum/n;
   var = (s2 - M*sum)/(n-1);

   ping_av = M;
   ping_sd = (int)sqrt((double)var);
}

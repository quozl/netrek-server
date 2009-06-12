/*
 * ping.c
 * 
 */
#include "copyright2.h"
#include "config.h"
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <math.h>
#include <errno.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"

/* file scope prototypes */
static void calc_loss(int i, struct ping_cpacket *packet);
static int mstime(void);
/* static int msetime(void); */
static void update_lag_stats(void);
static void update_loss_stats(void);
static int u_char_diff(int x, int y);

#define PING_DEBUG				0	/* debugging */
#define INL_STATS

static unsigned char ping_id;	/* wraparound expected */
static int      ping_lag;	/* ping roundtrip delay */

static double   tloss_sc,	/* total packet loss s-c */
                tloss_cs;	/* total packet loss c-s */
static int      iloss_sc,	/* inc. packet loss s-c */
                iloss_cs;	/* inc. packet loss c-s */

/*
 * This structure allows us to send several pings before any response is
 * received without losing information -- as would be the case for roundtrip
 * times equal to or larger then the ping interval times. HASHSIZE * ping
 * interval must be greater then the largest expected roundtrip time.
 */

#define HASHSIZE		32
#define PITH(i)			(((int)i) & (HASHSIZE-1))

static
struct {
   long            time;
   long            packets_sent_at_ping;
   int		   responded;	/* client replied to this ping */
}               ping_sent[HASHSIZE];

/*
 * response from client
 */

void pingResponse(struct ping_cpacket *packet)
{
   register int    i;
   static int      last_num;

   if (!ping || packet->pingme != 1)
      return;			/* oops, garbage */

   ping_ghostbust = 0;		/* don't ghostbust, client is alive */

   /* throw out out-of-order packets */
   i = u_char_diff((int) packet->number, last_num);
   if (i < 0) {
#if PING_DEBUG >= 1
      ERROR(1,( "out-of-order response ignored: %d (last: %d)\n",
	      packet->number, last_num));
      fflush(stderr);
#endif
      return;
   }
   if(ping_sent[PITH(packet->number)].responded){
#if PING_DEBUG >= 1
      ERROR(1,( "pingResponse: duplicate ping.\n")));
      fflush(stderr);
#endif
      return;
   }
   last_num = packet->number;
   i = PITH(last_num);
   ping_sent[i].responded = 1;

   /* calculate roundtrip */
   ping_lag = mstime() - ping_sent[i].time;
   if(ping_lag > 10000 || ping_lag < 0){
      /* sometimes a client will respond to very old pings due to a
	 network problem */
#if PING_DEBUG >= 1
      ERROR(1,( "Excess value (%d) thrown out.\n", ping_lag);
      fflush(stderr);
#endif
      ping_lag = me->p_avrt;
      return;
   }

#ifdef nodef
   /* SHORT PACKETS TEST */
   printf("%d: delay: %d\n", msetime(), ping_lag);
#endif

#ifdef INL_STATS
   /* fill in lag stats fields */
   update_lag_stats();
#endif

   /* watch out for div by 0 */
   if (!packets_received || !ping_sent[i].packets_sent_at_ping)
      return;

   /* calculate total packet loss */
   calc_loss(i, packet);

#ifdef INL_STATS
   update_loss_stats();
#endif
}

/*
 * request from server
 */

void sendClientPing(void)
{
   struct ping_spacket packet;

   ping_ghostbust++;
   ping_id++;			/* ok to wrap */

   packet.type = SP_PING;
   packet.number = (unsigned char) ping_id;
   packet.lag = htons((unsigned short) ping_lag);
   packet.tloss_sc = (int) round(tloss_sc);
   packet.tloss_cs = (int) round(tloss_cs);
   packet.iloss_sc = iloss_sc;
   packet.iloss_cs = iloss_cs;

   ping_sent[PITH(ping_id)].time = mstime();
   /*
    * printf("ping sent at %d\n", msetime());
    */

   sendClientPacket((struct player_spacket *)&packet);

   ping_sent[PITH(ping_id)].packets_sent_at_ping = packets_sent;
   ping_sent[PITH(ping_id)].responded = 0;
}

static void calc_loss(int i, struct ping_cpacket *packet)
{
   /* tloss vars */
   register int    cp_recv,	/* client packets recv */
                   cp_sent;	/* client packets sent */
   int             s_to_c_dropped,	/* server to client */
                   c_to_s_dropped;	/* client to server */
   static int      old_s_to_c_dropped,	/* previous update */
                   old_c_to_s_dropped;	/* "" */
   /* iloss vars */
   int             p_sent, p_recv;

   static
   int             timer;

   static int      inc_packets_sent,	/* packets sent start-point */
                   inc_packets_received,	/* packets recvd start-pt  */
                   inc_s_to_c_dropped,	/* dropped s-to-c start-pt */
                   inc_c_to_s_dropped;	/* dropped c-to-s start-pt */

   if (!timer)
      timer = ping_iloss_interval;

   cp_recv = ntohl(packet->cp_recv);
   cp_sent = ntohl(packet->cp_sent);

   /* at ping time, total packets dropped from server to client */
   s_to_c_dropped = ping_sent[i].packets_sent_at_ping - cp_recv;

   if (s_to_c_dropped < old_s_to_c_dropped) {
      /*
       * The network may duplicate or send out-of-order packets. Both are
       * detected and thrown out by the client if sequence checking is on.
       * If not there's not much we can do -- there's no way to distinguish a
       * duplicated packet from a series of out of order packets.  While the
       * latter case cancels itself out eventually in terms of packet loss,
       * the former hides real packet loss by adding extra packets. We'll
       * have to kludge it by adding the extra packets the client thinks it
       * got to packets_sent
       */
      packets_sent += old_s_to_c_dropped - s_to_c_dropped;
      /* and adjust s_to_c_dropped so we don't get a negative packet loss */
      s_to_c_dropped = old_s_to_c_dropped;
   }
   /* total loss server-to-client since start of connection */
   tloss_sc = 100 -
      (double) ((100 * (ping_sent[i].packets_sent_at_ping - s_to_c_dropped)) /
                (double) ping_sent[i].packets_sent_at_ping);

   /*
    * at ping time, total packets dropped from client to server NOTE: not
    * packets_received_at_ping since the client may have sent any amount of
    * packets between the time we sent the ping and the time the client
    * received it.
    */
   c_to_s_dropped = cp_sent - packets_received;

   /*
    * printf("cp_sent: %d, packets_received: %d\n", cp_sent,
    * packets_received);
    */

   if (c_to_s_dropped < old_c_to_s_dropped) {
      /*
       * The network may duplicate or send out-of-order packets. Since no
       * sequence checking is done by the server, there's not much we can do
       * -- there's no way to distinguish a duplicated packet from a series
       * of out of order packets.  While the latter case cancels itself out
       * eventually in terms of packet loss, the former hides real packet
       * loss by adding extra packets. We'll have to kludge it by subtracting
       * the extra packets we think we got from the client from
       * packets_received.
       */
      packets_received -= old_c_to_s_dropped - c_to_s_dropped;
      /* and adjust c_to_s_dropped so we don't get a negative packet loss */
      c_to_s_dropped = old_c_to_s_dropped;
   }
   /* total loss client-to-server since start of connection */
   tloss_cs = 100 -
      (double) ((100 * (packets_received - c_to_s_dropped)) /
                (double) packets_received);

   old_s_to_c_dropped = s_to_c_dropped;
   old_c_to_s_dropped = c_to_s_dropped;

   /* Incremental packet loss */

   /* packets sent since last ping response */
   p_sent = ping_sent[i].packets_sent_at_ping - inc_packets_sent;

   /* packets received since last ping response */
   p_recv = packets_received - inc_packets_received;

   if (!p_sent || !p_recv) {
      /* just in case */
      return;
   }
   /* percent loss server-to-client since PACKET_LOSS_INTERVAL */
   iloss_sc = 100 -
      (int)((100 * (p_sent - (s_to_c_dropped - inc_s_to_c_dropped))) / 
	    (double)p_sent);
   /*
    * we're not going to do any of the adjustments we did in tloss
    * calculations since this starts fresh every PACKET_LOSS_INTERVAL
    */
   if (iloss_sc < 0)
      iloss_sc = 0;

   /* total percent loss client-to-server since PACKET_LOSS_INTERVAL */
   iloss_cs = 100 -
      (int)((100 * (p_recv - (c_to_s_dropped - inc_c_to_s_dropped))) / 
	     (double)p_recv);
   /*
    * we're not going to do any of the adjustments we did in tloss
    * calculations since this starts fresh every PACKET_LOSS_INTERVAL
    */
   if (iloss_cs < 0)
      iloss_cs = 0;

   /*
    * we update these variables every PACKET_LOSS_INTERVAL seconds to start a
    * fresh increment
    */
   if ((timer % ping_iloss_interval) == 0) {
      inc_s_to_c_dropped = s_to_c_dropped;
      inc_c_to_s_dropped = c_to_s_dropped;

      inc_packets_sent = ping_sent[i].packets_sent_at_ping;
      inc_packets_received = packets_received;
   }
   timer++;
}

#ifdef INL_STATS

/*
 * INL lag stats struct player .p_avrt -	average round trip time ms
 * struct player .p_stdv - 	standard deviation in rt time struct player
 * .p_pkls -	input/output packet loss
 */

static double   s2;
static int      sum, n;
static int      M, var;

static void update_lag_stats(void)
{
   n++;
   sum += ping_lag;
   s2 += (ping_lag * ping_lag);
   if (n == 1)
      return;

   M = sum / n;
   var = (int)((double) (s2 - (double)(M * sum)) / (double)(n - 1));
   if(var < 0){
      ERROR(1,( "%s@%s: var: %d, n: %d, sum: %d, s2: %f (%d) (reset)\n",
	 login, host, var, n, sum, s2, ping_lag));
      ERROR(1,( "     at time %d\n", mstime()));
      fflush(stderr);
      n = 1;
      sum = 0;
      s2 = 0.0;
      return;
   }

   /* average round trip time */
   me->p_avrt = M;
   /* standard deviation */
   me->p_stdv = (int) sqrt((double) var);
}

static void update_loss_stats(void)
{
   me->p_pkls_s_c = tloss_sc;
   me->p_pkls_c_s = tloss_cs;
}
#endif				/* INL_STATS */

/* utilities */

/* ms time from start */
static int mstime(void)
{
   static struct timeval  tv_base = {0,0};
   struct timeval  tv;

   if (!tv_base.tv_sec) {
      gettimeofday(&tv_base, NULL);
      return 0;
   }
   gettimeofday(&tv, NULL);
   return (tv.tv_sec - tv_base.tv_sec) * 1000 +
      (tv.tv_usec - tv_base.tv_usec) / 1000;
}

/* debugging */
/* static int msetime(void) */
/* { */
/*    struct timeval  tv; */
/*    gettimeofday(&tv, NULL); */
/*    return (tv.tv_sec - 732737182) * 1000 + tv.tv_usec / 1000; */
/* } */

static int u_char_diff(int x, int y)
{
   register int res;

   res = x - y;

   if (res > 128)
      return res - 256;
   else if (res < -128)
      return res + 256;
   else
      return res;
}

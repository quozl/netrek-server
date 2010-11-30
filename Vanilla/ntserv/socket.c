/*
 * Socket.c 
 *
 * Kevin P. Smith  1/26/89
 * UDP stuff v1.0 by Andy McFadden  Feb-Apr 1992
 *
 * UDP protocol v1.0
 *
 * Provides all of the support for sending packets to the client.
 */
#include "copyright2.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "sigpipe.h"
#include "ltd_stats.h"
#include "ip.h"
#include "util.h"
#include "genspkt.h"
#include "slotmaint.h"
#include "draft.h"

#ifdef OOPS
#ifdef MIPSEL
#ifndef htonl

#define htonl(x) ((U_LONG) (((U_LONG) x << 24) | (((U_LONG) x & 0xff00) << 8) | (((U_LONG) x & 0xff0000) >> 8) | ((U_LONG) x >> 24)))
#define ntohl(x) htonl(x)
#define htons(x) ((u_short) (((u_short) x << 8) | ((u_short) x >> 8)))
#define ntohs(x) htons(x)

#endif /* htonl */
#endif /* MIPSEL */
#endif /* OOPS */

#undef DOUBLE_UDP	/* comment this out to disable it */

#undef BROKEN		/* for local stress testing; drops 20% of packets */
#undef HOSED		/* combine with BROKEN; drops 90% of packets */
#undef FATDIAG		/* define to get LOTS of fat UDP debugging messages */

/* file scope prototypes */

static void handleTorpReq(struct torp_cpacket *packet);
static void handlePhasReq(struct phaser_cpacket *packet);
static void handleSpeedReq(struct speed_cpacket *packet);
static void handleDirReq(struct dir_cpacket *packet);
static void handleShieldReq(struct shield_cpacket *packet);
static void handleRepairReq(struct repair_cpacket *packet);
static void handleOrbitReq(struct orbit_cpacket *packet);
static void handlePractrReq(struct practr_cpacket *packet);
static void handleBombReq(struct bomb_cpacket *packet);
static void handleBeamReq(struct beam_cpacket *packet);
static void handleCloakReq(struct cloak_cpacket *packet);
static void handleDetTReq(struct det_torps_cpacket *packet);
static void handleCopilotReq(struct copilot_cpacket *packet);
static void handleOutfit(struct outfit_cpacket *packet);
static void handleLoginReq(struct login_cpacket *packet);
static void handlePlasmaReq(struct plasma_cpacket *packet);
static void handleWarReq(struct war_cpacket *packet);
static void handlePlanReq(struct planet_cpacket *packet);
static void handlePlanlockReq(struct planlock_cpacket *packet);
static void handlePlaylockReq(struct playlock_cpacket *packet);
static void handleDetMReq(struct det_mytorp_cpacket *packet);
static void handleTractorReq(struct tractor_cpacket *packet);
static void handleRepressReq(struct repress_cpacket *packet);
static void handleCoupReq(struct coup_cpacket *packet);
static void handleRefitReq(struct refit_cpacket *packet);
static void handleMessageReq(struct mesg_cpacket *packet);
static void handleQuitReq(struct quit_cpacket *packet);
static void handleOptionsPacket(struct options_cpacket *packet);
static void handleSocketReq(struct socket_cpacket *packet);
static void handleByeReq(struct bye_cpacket *packet);
static void handleDockingReq(struct dockperm_cpacket *packet);
static void handleReset(struct resetstats_cpacket *packet);
static void handleUpdatesReq(struct updates_cpacket *packet);
static void handleReserved(struct reserved_cpacket *packet);
static void handleScan(struct scan_cpacket *packet);
static void handleUdpReq(struct udp_req_cpacket *packet);
static void handleSequence(void);
static void handleShortReq(struct shortreq_cpacket *packet);
static void handleThresh(struct threshold_cpacket *packet);
static void handleSMessageReq(struct mesg_s_cpacket *packet);

#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
static void handleOggV(struct oggv_cpacket *packet);
#endif
static void handleFeature(struct feature_cpacket *cpacket);

extern int ignored[];

#ifdef RSA
static void handleRSAKey();
#endif

static void handlePingResponse(struct ping_cpacket  *packet);
static void clientVersion(struct mesg_spacket *packet);
static int doRead(int asock);
static int gwrite(int fd, char *wbuf, size_t size);
static void logmessage(char *string);
static int connUdpConn(void);
static void printUdpInfo(void);
#ifdef DOUBLE_UDP
static void sendSC(void);
#endif
static void forceUpdate(void);
static void updateFat(struct player_spacket *packet);
static void fatten(void);
static void fatMerge(void);
static int parseIgnore(struct mesg_cpacket *packet);
static int check_mesgs(struct mesg_cpacket  *packet);
static void check_clue(struct mesg_cpacket  *packet);

struct packet_handler {
    int size;
    void (*handler)();
};

struct packet_handler handlers[] = {
    { 0, NULL },		/* record 0 */
    { sizeof(struct mesg_cpacket), handleMessageReq },	   /* CP_MESSAGE */
    { sizeof(struct speed_cpacket), handleSpeedReq },	   /* CP_SPEED */
    { sizeof(struct dir_cpacket), handleDirReq },	   /* CP_DIRECTION */
    { sizeof(struct phaser_cpacket), handlePhasReq },	   /* CP_PHASER */
    { sizeof(struct plasma_cpacket), handlePlasmaReq },	   /* CP_PLASMA */
    { sizeof(struct torp_cpacket), handleTorpReq },	   /* CP_TORP */
    { sizeof(struct quit_cpacket), handleQuitReq },	   /* CP_QUIT */
    { sizeof(struct login_cpacket), handleLoginReq },	   /* CP_LOGIN */
    { sizeof(struct outfit_cpacket), handleOutfit },	   /* CP_OUTFIT */
    { sizeof(struct war_cpacket), handleWarReq },	   /* CP_WAR */
    { sizeof(struct practr_cpacket), handlePractrReq },	   /* CP_PRACTR */
    { sizeof(struct shield_cpacket), handleShieldReq },	   /* CP_SHIELD */
    { sizeof(struct repair_cpacket), handleRepairReq },	   /* CP_REPAIR */
    { sizeof(struct orbit_cpacket), handleOrbitReq },	   /* CP_ORBIT */
    { sizeof(struct planlock_cpacket), handlePlanlockReq },/* CP_PLANLOCK */
    { sizeof(struct playlock_cpacket), handlePlaylockReq },/* CP_PLAYLOCK */
    { sizeof(struct bomb_cpacket), handleBombReq },	   /* CP_BOMB */
    { sizeof(struct beam_cpacket), handleBeamReq },	   /* CP_BEAM */
    { sizeof(struct cloak_cpacket), handleCloakReq },	   /* CP_CLOAK */
    { sizeof(struct det_torps_cpacket), handleDetTReq },   /* CP_DET_TORPS */
    { sizeof(struct det_mytorp_cpacket), handleDetMReq },  /* CP_DET_MYTORP */
    { sizeof(struct copilot_cpacket), handleCopilotReq },  /* CP_COPLIOT */
    { sizeof(struct refit_cpacket), handleRefitReq }, 	   /* CP_REFIT */
    { sizeof(struct tractor_cpacket), handleTractorReq },  /* CP_TRACTOR */
    { sizeof(struct repress_cpacket), handleRepressReq },  /* CP_REPRESS */
    { sizeof(struct coup_cpacket), handleCoupReq },	   /* CP_COUP */
    { sizeof(struct socket_cpacket), handleSocketReq },	   /* CP_SOCKET */
    { sizeof(struct options_cpacket), handleOptionsPacket },/* CP_OPTIONS */
    { sizeof(struct bye_cpacket), handleByeReq },	   /* CP_BYE */
    { sizeof(struct dockperm_cpacket), handleDockingReq }, /* CP_DOCKPERM */
    { sizeof(struct updates_cpacket), handleUpdatesReq 	}, /* CP_UPDATES */
    { sizeof(struct resetstats_cpacket), handleReset },    /* CP_RESETSTATS */
    { sizeof(struct reserved_cpacket), handleReserved },   /* CP_RESERVED */
    { sizeof(struct scan_cpacket), handleScan },	   /* CP_SCAN (ATM) */
    { sizeof(struct udp_req_cpacket), handleUdpReq },	   /* CP_UDP_REQ */
    { sizeof(struct sequence_cpacket), handleSequence },   /* CP_SEQUENCE */
#ifdef RSA
    { sizeof(struct rsa_key_cpacket), handleRSAKey },	   /* CP_RSA_KEY */
#else
    { 0, NULL },					   /* 37 */
#endif
    { sizeof(struct planet_cpacket), handlePlanReq },	   /* CP_PLANET */
    { 0, NULL },					   /* 39 */
    { 0, NULL },					   /* 40 */
    { 0, NULL },					   /* 41 */
    { sizeof(struct ping_cpacket), handlePingResponse },   /* CP_PING_RESPONSE*/
    { sizeof(struct shortreq_cpacket), handleShortReq },   /* CP_S_REQ */
    { sizeof(struct threshold_cpacket), handleThresh },    /* CP_S_THRS */
    { -1, handleSMessageReq},                              /* CP_S_MESSAGE */
    { 0, NULL },					   /* 46 */
    { 0, NULL },					   /* 47 */
    { 0, NULL },					   /* 48 */
    { 0, NULL },					   /* 49 */
#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
    { sizeof(struct oggv_cpacket), handleOggV },           /* CP_OGGV */
#else
    { 0, NULL },					   /* 50 */
#endif
    { 0, NULL },					   /* 51 */
    { 0, NULL },					   /* 52 */
    { 0, NULL },					   /* 53 */
    { 0, NULL },					   /* 54 */
    { 0, NULL },					   /* 55 */
    { 0, NULL },					   /* 56 */
    { 0, NULL },					   /* 57 */
    { 0, NULL },					   /* 58 */
    { 0, NULL },					   /* 59 */
    { sizeof(struct feature_cpacket), handleFeature },	   /* CP_FEATURE */
};

extern int sizes[TOTAL_SPACKETS];
#define NUM_PACKETS (sizeof(handlers) / sizeof(handlers[0]) - 1)
#define NUM_SIZES (sizeof(sizes) / sizeof(sizes[0]) - 1)

int packetsReceived[NUM_PACKETS+1] = { 0 };
int packetsSent[NUM_SIZES+1] = { 0 };

#define BUFSIZE 16738
#define UDPBUFSIZE 1024			/* (tweakable; should be under 1300) */
static char buf[BUFSIZE];		/* Socket buffer */
static char udpbuf[UDPBUFSIZE];		/* UDP socket buffer */
static char *bufptr=buf;
static char *udpbufptr=udpbuf;
#ifdef DOUBLE_UDP
static char scbuf[UDPBUFSIZE];		/* semi-critical UDP socket buffer */
static char *scbufptr=scbuf;		/* (only used for double UDP) */
#endif
int clientDead=0;
static LONG sequence;			/* the holy sequence number */

static int portswapflags = 0;

#define PORTSWAP_ENABLED 1
#define PORTSWAP_CONNECTED 2
#define PORTSWAP_UDPRECEIVED 4


static int udpLocalPort = 0;
static int udpClientPort = 0;
static int udpMode = MODE_SIMPLE;	/* what kind of UDP trans we want */

#ifdef UDP_FIX 
static struct sockaddr_in caddr;        /* 18/8/92 SYSV time out fix SK */
#endif

/* this stuff is used for Fat UDP */
typedef void * PTR;			/* adjust this if you lack (void *) */
typedef struct fat_node_t {
    PTR packet;
    int pkt_size;
    struct fat_node_t *prev;
    struct fat_node_t *next;
} FAT_NODE;


static void dequeue(FAT_NODE *fatp);
static void enqueue(FAT_NODE *fatp, int list);

/* needed for fast lookup of semi-critical fat nodes */
FAT_NODE fat_kills[MAXPLAYER];
FAT_NODE fat_torp_info[MAXPLAYER * MAXTORP];
FAT_NODE fat_phaser[MAXPLAYER];
FAT_NODE fat_plasma_info[MAXPLAYER * MAXPLASMA];
FAT_NODE fat_you;
FAT_NODE fat_s_you;
FAT_NODE fat_s_planet;
FAT_NODE fat_s_kills;	/* S_P2 */
FAT_NODE fat_s_phaser[MAXPLAYER]; 
FAT_NODE fat_status;
FAT_NODE fat_planet[MAXPLANETS];
FAT_NODE fat_flags[MAXPLAYER];
FAT_NODE fat_hostile[MAXPLAYER];
/* define the lists */
#define MAX_FAT_LIST	5		/* tweakable; should be > 1 */
typedef struct {
    FAT_NODE *head;
    FAT_NODE *tail;
} FAT_LIST;
FAT_LIST fatlist[MAX_FAT_LIST], tmplist[MAX_FAT_LIST];
/* tweakable parameters; compare with UDPBUFSIZE */
/* NOTE: FAT_THRESH + MAX_FAT_DATA must be < UDPBUFSIZE                */
/*       MAX_FAT_DATA must be larger than biggest semi-critical packet */
#define FAT_THRESH	500		/* if more than this, don't add fat */
#define MAX_FAT_DATA	100		/* add at most this many bytes */
#define MAX_NONFAT	10		/* if we have this much left, stop */

static FILE	*mlog;

/*int           send_short        = 0;*/ /* is now in data.c , because of robotII.c */
/* static int	send_mesg         = 1; */
/* static int	send_kmesg        = 1; */
/* static int	send_warn         = 1; */

static int	spk_update_sall   = 0;	/* Small Update: Only weapons, */
/* Kills and Planets */
static int	spk_update_all	  = 0;  /* Full Update minus SP_STATS */

#define         SPK_VOFF	0	/* variable packets off */
#define         SPK_VON		1	/* variable packets on */
#define         SPK_MOFF	2	/* message packets off */
#define         SPK_MON		3	/* message packets on */
#define         SPK_M_KILLS	4
#define         SPK_M_NOKILLS	5
#define         SPK_THRESHOLD	6
#define         SPK_M_WARN	7
#define         SPK_M_NOWARN	8
#define 	SPK_SALL	9	/* only planets,kills and weapons */
#define         SPK_ALL		10	/* Full Update - SP_STATS */


int use_newyou=0;
int f_many_self=0;

/* S_P2 */
#define SizeOfUDPUpdate()               (udpbufptr-udpbuf)

void initClientData(void);
void updateStatus(int force);
void updateSelf(int force);
void updateShips(void);
void updatePhasers(void);
void updateTorps(void);
void updatePlasmas(void);

static char *whoami(void)
{
    if (me == NULL) return "Q?";
    return me->p_mapchars;
}

void setNoDelay(int fd)
{
    int status;
    int option_value = 1;
    
    status = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, 
			  (char *) &option_value, sizeof(option_value));
    if (status < 0) { 
	ERROR(1,("%s: setsockopt() failed, %s\n", whoami(), 
		 strerror(errno)));
	/* can still play without this, proceed */
    }
}

int connectToClient(char *machine, int port)
{
    int ns, stat, derrno;
    socklen_t derrnol;
    struct sockaddr_in addr;
    struct hostent *hp;
    struct timeval timeout;
    fd_set writefds;

    if (sock!=-1) {
	shutdown(sock,2);
	sock= -1;
    }
    ERROR(3,("%s: start connect to %s:%d\n", whoami(), machine, port)); 

    if ((ns=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	ERROR(1,("%s: socket() failed, %s\n", whoami(), strerror(errno)));
	exit(2);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (remoteaddr != -1) {
	addr.sin_addr.s_addr = remoteaddr;
    } else if ((addr.sin_addr.s_addr = inet_addr(machine)) == -1) {
	if ((hp = gethostbyname(machine)) == NULL) {
	    ERROR(1,("%s: gethostbyname() failed, %s\n", whoami(), 
		     strerror(errno)));
	    close(ns);
	    exit(1);
	} else {
	    addr.sin_addr.s_addr = *(U_LONG *) hp->h_addr;
	}
    }
    remoteaddr = addr.sin_addr.s_addr;
    ip = strdup(inet_ntoa(addr.sin_addr));

    /* 18th Feb 1999, cameron@stl.dec.com, suspect this is where the large
       delay is for a ghostbusted slot, assuming usual PPP dial-in user, this
       connect call can block for three minutes or so because the host just
       plain isn't there any more.  One solution is to set the socket to
       non-blocking, issue the connect, then use select to timeout on a shorter
       interval, and cancel the connect after that interval */

    /* set the socket non-blocking */
    stat = ioctl(ns, FIONBIO, "in");
    if (stat < 0) {
	ERROR(1,("%s: ioctl(FIONBIO) failed, %s\n", whoami(), 
		 strerror(errno)));
	close(ns);
	return 0;
    }

    /* start a connection attempt */
    stat = connect(ns, (struct sockaddr *) &addr, sizeof(addr));
    if (stat < 0 && errno != EINPROGRESS) {
	ERROR(2,("%s: connect() failed, %s\n", whoami(), 
		 strerror(errno)));
	close(ns);
	return 0;
    }

    /* wait a short time for it to complete */
    timeout.tv_sec=10;
    timeout.tv_usec=0;
    FD_ZERO(&writefds);
    FD_SET(ns, &writefds);
    stat = select(ns+1, NULL, &writefds, NULL, &timeout); 
    if (stat < 0) {
	ERROR(1,("%s: select() after connect() failed, %s\n", whoami(), 
		 strerror(errno)));
	close(ns);
	return 0;
    }
    if (stat == 0) {
	ERROR(3,("%s: connect timed out\n", whoami()));
	close(ns);
	return 0;
    }

    /* read status of connect attempt */
    derrnol = sizeof(derrno);
    stat = getsockopt(ns, SOL_SOCKET, SO_ERROR, &derrno, &derrnol);
    if (stat < 0) {
	derrno = errno;
    }

    switch (derrno) {
    case 0:
	break;
    case ECONNREFUSED:
	ERROR(3,("%s: connection refused\n", whoami()));
	close(ns);
	return 0;
    case ENETUNREACH:
	ERROR(3,("%s: network unreachable\n", whoami()));
	close(ns);
	return 0;
    case ETIMEDOUT:
    default:
	ERROR(3,("%s: connect() failed, %s\n", whoami(), 
		 strerror(errno)));
	close(ns);
	return 0;
    }

    /* set the socket blocking */
    stat = ioctl(ns, FIONBIO, "out");
    if (stat < 0) {
	ERROR(1,("%s: ioctl(FIONBIO) failed, %s\n", whoami(), 
		 strerror(errno)));
	close(ns);
	return 0;
    }

    sock=ns;
    setNoDelay(sock);
    initClientData();
    testtime = -1;
    return 1;
}

/* Check the socket to read it's inet addr for possible future use.
 */
void checkSocket(void)
{
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getpeername(sock, (struct sockaddr *) &sin, &len) < 0) {
	return;
    }
    remoteaddr = sin.sin_addr.s_addr;
    ip = strdup(inet_ntoa(sin.sin_addr));
}

void initClientData(void)
    /* invalidates all data, so it is all sent to the client */
{
    int i;

    if (logall) {
	if (!mlog) mlog = fopen(MesgLog, "a");
	if (!mlog) perror(MesgLog);
    }
    if (loggod) glog_open();

    clientDead=0;

    /* Much of the code moved to genspkt.c */
    initSPackets();

    for (i=0; i<MAXPLAYER; i++) {
	extern struct hostile_spacket clientHostile[MAXPLAYER];
	extern struct phaser_spacket clientPhasers[MAXPLAYER];
	extern struct phaser_s_spacket client_s_Phasers[MAXPLAYER];
	extern struct kills_spacket clientKills[MAXPLAYER];
	extern struct flags_spacket clientFlags[MAXPLAYER];

	fat_hostile[i].packet = (PTR) &clientHostile[i];
	fat_hostile[i].pkt_size = sizeof(struct hostile_spacket);
	fat_hostile[i].prev = fat_hostile[i].next = (FAT_NODE *) NULL;
	fat_phaser[i].packet = (PTR) &clientPhasers[i];
	fat_phaser[i].pkt_size = sizeof(struct phaser_spacket);
	fat_phaser[i].prev = fat_phaser[i].next = (FAT_NODE *) NULL;
	fat_s_phaser[i].packet = (PTR) & client_s_Phasers[i];
	fat_s_phaser[i].pkt_size = sizeof (struct phaser_s_spacket);
	fat_s_phaser[i].prev = fat_s_phaser[i].next = (FAT_NODE *) NULL;
	fat_kills[i].packet = (PTR) &clientKills[i];
	fat_kills[i].pkt_size = sizeof(struct kills_spacket);
	fat_kills[i].prev = fat_kills[i].next = (FAT_NODE *) NULL;
	fat_flags[i].packet = (PTR) &clientFlags[i];
	fat_flags[i].pkt_size = sizeof(struct flags_spacket);
	fat_flags[i].prev = fat_flags[i].next = (FAT_NODE *) NULL;
    }
    for (i=0; i<MAXPLAYER*MAXTORP; i++) {
	extern struct torp_info_spacket clientTorpsInfo[MAXPLAYER*MAXTORP];

	fat_torp_info[i].packet = (PTR) &clientTorpsInfo[i];
	fat_torp_info[i].pkt_size = sizeof(struct torp_info_spacket);
	fat_torp_info[i].prev = fat_torp_info[i].next = (FAT_NODE *) NULL;
    }
    for (i=0; i<MAXPLAYER*MAXPLASMA; i++) {
	extern struct plasma_info_spacket clientPlasmasInfo[MAXPLAYER*MAXPLASMA];

	fat_plasma_info[i].packet = (PTR) &clientPlasmasInfo[i];
	fat_plasma_info[i].pkt_size = sizeof(struct plasma_info_spacket);
	fat_plasma_info[i].prev = fat_torp_info[i].next = (FAT_NODE *) NULL;
    }
    for (i=0; i<MAXPLANETS; i++) {
	extern struct planet_spacket clientPlanets[MAXPLANETS];

	fat_planet[i].packet = (PTR) &clientPlanets[i];
	fat_planet[i].pkt_size = sizeof(struct planet_spacket);
	fat_planet[i].prev = fat_planet[i].next = (FAT_NODE *) NULL;
    }

    {
	extern struct you_short_spacket	clientSelfShort;
	extern u_char clientVPlanets[];
	extern u_char clientVKills[];
	extern struct you_spacket clientSelf;
	extern struct status_spacket clientStatus;
/*	extern struct you_new_spacket clientSelf3;*/

	fat_s_you.packet = (PTR) &clientSelfShort;
	fat_s_you.pkt_size = sizeof(struct you_short_spacket);
	fat_s_you.prev = fat_s_you.next = (FAT_NODE *) NULL;
	fat_s_planet.packet = (PTR) clientVPlanets;
	fat_s_planet.pkt_size = 0;	/* It's dynamic */
	fat_s_planet.prev =  fat_s_planet.next = (FAT_NODE *) NULL;
	/* S_P2 */
	fat_s_kills.packet = (PTR) clientVKills;
	fat_s_kills.pkt_size = 0;
	fat_s_kills.prev = fat_s_kills.next = (FAT_NODE *) NULL;

	fat_you.packet = (PTR) &clientSelf;
	fat_you.pkt_size = sizeof(struct you_spacket);
	fat_you.prev = fat_you.next = (FAT_NODE *) NULL;
	fat_status.packet = (PTR) &clientStatus;
	fat_status.pkt_size = sizeof(struct status_spacket);
	fat_status.prev = fat_status.next = (FAT_NODE *) NULL;
    }

    for (i = 0; i < MAX_FAT_LIST; i++)
	fatlist[i].head = fatlist[i].tail = (FAT_NODE *) NULL;
}

int isClientDead(void)
{
    return clientDead;
}

void updateClient(void)
{
    int threshold_1 = 60 + COST_GENERIC_32;

#ifdef SHORT_THRESHOLD
    static int skip = 0;	/* If skip is set we skip next update */
    /* This can halve your updates */
    if (send_short && skip) {
	skip = 0;		/* back to default */
	if (bufptr==buf && (commMode!=COMM_UDP || udpbufptr==buf)) {
	    /* We sent nothing!  We better send something to wake him */
	    updateSelf(TRUE);
	}
	flushSockBuf();
	repCount++;
	return;
    }
#endif

    /* This moved from its mysterious location in updateMessages to here */
    /* It checks to see if the system defaults file has changed and reopens
       the log files if necessary.  What this has to do with sending messages -
       I dont know. */
    update_sys_defaults();
    if (logall) {
        if (!mlog) mlog = fopen(MesgLog, "a");
        if (!mlog) perror(MesgLog);
    }
    if (loggod) glog_open();

    if(commMode == COMM_UDP) addSequenceFlags(udpbuf);
    if(send_short) {
	updatePlasmas();
	updateSelf(FALSE);
	updatePhasers();
	updateShips();
	if (F_full_weapon_resolution) {
	    SupdateTorps();
	    updateTorps();
	}
	else
	    SupdateTorps();
	updatePlanets();
	updateMessages();
	/* EXPERIMENT:  Don't inflate large packet with non-crucial stuff  S_P2 */
	if(F_full_direction_resolution || F_full_weapon_resolution
	  || SizeOfUDPUpdate() < threshold_1)
	    updateStatus(TRUE);
	else
	    updateStatus(FALSE);  /* Update only if status->torn changes */
	updatePlayerStats();
    } else {
	updateTorps();
	updatePlasmas();
	updateStatus(TRUE);
	updateSelf(FALSE);
	updatePhasers();
	updateShips();
	updatePlanets();
	updateMessages();
	/* S_P2 */
	updatePlayerStats();
    }

    last_udp_size = SizeOfUDPUpdate();
    if (bufptr==buf && (commMode!=COMM_UDP || udpbufptr==buf)) {
	/* We sent nothing!  We better send something to wake him */
	updateSelf(TRUE);
    }

#ifdef SHORT_THRESHOLD
    if ((actual_threshold != 0 ) && (((bufptr-buf) + (udpbufptr-udpbuf)) 
				     > actual_threshold))
	skip = 1;
    /* Next update is skipped */
    /* Question: Should i only test udpbuffer, or separate both? */
#endif

    /* NOTE: this must be the last thing we do before flushing
       the socket buffer. */
    if (ping && (repCount % efticks(5*ping_freq) == 0) && testtime == 0 &&
       /* these checks are here to make sure we don't ping from the
	  updateClient call in death().  The reason is that savestats()
	  can take > 100ms, invalidating the next ping lag calc */
       (me->p_status == PALIVE || me->p_status == POBSERV))
        sendClientPing();                /* ping.c */
    flushSockBuf();
    repCount++;
}

void sendClientPacket(void *void_packet)
{
    struct player_spacket *packet = (struct player_spacket *) void_packet;
    int 		orig_type;
    int 		size, issc;
    size_t 		t;
#ifdef MAYBE
    static int		oldstatus = POUTFIT;
#endif

    orig_type = packet->type;
/*    packet->type &= ~(0x40 | 0x80); */	/* clear special flags */
    packet->type &= (char) 0x3f;    /* above doesn't work? 4/18/92 TC */
#ifdef MAYBE
    /*
     * If we're dead, dying, or just born, we definitely want the transmission
     * to get through (otherwise we can get stuck).  I don't think this will
     * be a problem for anybody, though it might hang for a bit if the TCP
     * connection is bad.
     */
    /* Okay, now I'm not so sure.  Whatever. */
    if (oldstatus != PALIVE || (me != NULL && me->p_status != PALIVE))
	orig_type = packet->type | 0x80;	/* pretend it's critical */
#endif
    if (packet->type<1 || packet->type>NUM_SIZES || 
	sizes[(int)packet->type]==0) {
	ERROR(1,("%s: attempt to send strange packet %d %d\n", whoami(),
		 packet->type, (int)NUM_SIZES));
	return;
    }
    packetsSent[(int)packet->type]++;
    if (commMode == COMM_TCP || (commMode == COMM_UDP && udpMode == MODE_TCP)) {
	/*
	 * business as usual
	 */
	size=(int) sizes[(int)packet->type];
	if (bufptr-buf+size >= BUFSIZE) {
	    t=bufptr-buf;
	    if (gwrite(sock, buf, t) != t) {
		ERROR(1,("%s: gwrite(TCP) failed, %s, client marked dead\n",
		       whoami(), strerror(errno)));
		clientDead=1;
	    }
	    bufptr=buf;
	}
	memcpy(bufptr, packet, size);
	bufptr+=size;

    } else {
	/*
	 * do UDP stuff unless it's a "critical" packet
	 * (note that both kinds get a sequence number appended) (FIX)
	 */
	issc = 0;
	switch (orig_type) {
	case SP_KILLS:
	case SP_TORP_INFO:
	case SP_PHASER:
	case SP_PLASMA_INFO:
	case SP_YOU|0x40:	/* 0x40 if semi-critical packet */
/*	case SP_STATUS:		S_P2 */
	case SP_PLANET:
	case SP_FLAGS:
	case SP_HOSTILE:
	case SP_S_YOU|0x40:     /* 0x40 if semi-critical packet */
	case SP_S_PLANET:
        case SP_S_PHASER:       /* S_P2 */
	case SP_S_KILLS:	/* S_P2 */
	case SP_S_YOU_SS:
	case SP_S_PLAYER:
	case SP_S_TORP:
	case SP_S_TORP_INFO:
	case SP_S_8_TORP:
	case SP_S_STATS:	/* S_P2 */

	case SP_PLAYER:
	case SP_TORP:
	case SP_YOU:
	case SP_PLASMA:
	case SP_STATS:
/*	case SP_SCAN:*/
	case SP_STATUS:	/* S_P2 */
        case SP_PING:
	case SP_UDP_REPLY:	/* only reply when COMM_UDP is SWITCH_VERIFY */
	case SP_GENERIC_32:
	case SP_FLAGS_ALL:
	    /* these are non-critical updates; send them via UDP */
	    V_UDPDIAG(("Sending type %d\n", (int)packet->type));
	    size=sizes[(int)packet->type];
            packets_sent++;
	    if (udpbufptr-udpbuf+size >= UDPBUFSIZE) {
		addSequenceFlags(udpbuf);
		t=udpbufptr-udpbuf;
		if (gwrite(udpSock, udpbuf, t) != t) {
		    ERROR(1,("%s: gwrite(UDP) failed, %s, client marked dead once more\n",
			     whoami(), strerror(errno)));
#ifdef EXTRA_GB
		    clientDead=1;
#endif
		    UDPDIAG(("*** UDP disconnected for %s\n", me->p_name));
                    printUdpInfo();
                    closeUdpConn();
                    commMode = COMM_TCP;
		}
#ifdef DOUBLE_UDP
		sendSC();	/* send semi-critical info, if needed */
#endif
		udpbufptr = udpbuf + addSequence(udpbuf, &sequence);
	    }
	    memcpy(udpbufptr, packet, size);
	    udpbufptr+=size;
#ifdef DOUBLE_UDP
	    if (issc && udpMode == MODE_DOUBLE) {
		memcpy(scbufptr, packet, size);
		scbufptr+=size;
		V_UDPDIAG((" adding SC\n"));
	    }
#endif
	    if (issc && udpMode == MODE_FAT) {
		updateFat(packet);
	    }
	    break;

	default:
	    /* these are critical packets; send them via TCP */
	    size=sizes[(int)packet->type];
	    if (bufptr-buf+size >= BUFSIZE) {
		t=bufptr-buf;
		if (gwrite(sock, buf, t) != t) {
		    ERROR(1,("%s: critical gwrite(TCP) failed, %s, client marked dead\n",
			     whoami(), strerror(errno)));
		    clientDead=1;
		}
		bufptr=buf /*+ addSequence(buf)*/;
	    }
	    memcpy(bufptr, packet, size);
	    bufptr+=size;
	    break;
	}
    }
}

/* Flush the socket buffer */
void flushSockBuf(void)
{
    size_t t;

    if (clientDead){
	return;
    }
    if (bufptr != buf) {
	t = bufptr - buf; /* GCC 2.4.5 Optimizer Bug don't touch this */
	if (gwrite(sock, buf, t) != t) {
	    ERROR(1,("%s: flush gwrite(TCP) failed, %s, client marked dead\n",
		     whoami(), strerror(errno)));
	    clientDead=1;
	}
	bufptr=buf /*+ addSequence(buf)*/;
    }
    /*
     * This is where we try to add fat.  There's no point in checking at
     * the other places which call gwrite(), because they only call it when
     * the buffer is already full.
     */
    if (udpSock >= 0 && udpMode == MODE_FAT && (udpbufptr-udpbuf)<FAT_THRESH)
	fatten();
    if (udpSock >= 0 && udpbufptr != udpbuf) {
#ifdef BROKEN
	/* debugging only!! */
# ifdef HOSED
	if (sequence % 10)
# else
	    if (sequence % 5 == 0)
# endif
	    {
		/* act as if we did the gwrite(), but don't */
		udpbufptr=udpbuf + addSequence(udpbuf, &sequence);
		goto foo;
	    }
#endif
	t=udpbufptr-udpbuf;
	if (gwrite(udpSock, udpbuf, t) != t){
	    ERROR(1,("%s: flush gwrite(UDP) failed, %s, client marked dead\n",
		     whoami(), strerror(errno)));
#ifdef EXTRA_GB
	    clientDead=1;
#endif
	    UDPDIAG(("*** UDP disconnected for %s\n", me->p_name));
	    printUdpInfo();
	    closeUdpConn();
	    commMode = COMM_TCP;
	}
#ifdef DOUBLE_UDP
	sendSC();
#endif
	udpbufptr=udpbuf + addSequence(udpbuf, &sequence);
    }
#ifdef BROKEN
foo:
#endif
    if (udpMode == MODE_FAT)
	fatMerge();
}

void socketPause(int sec)
{
    struct timeval timeout;
    fd_set readfds;

    timeout.tv_sec=sec;
    timeout.tv_usec=0;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    if (udpSock >= 0)
	FD_SET(udpSock, &readfds);

    select(MAX(sock, udpSock)+1, &readfds, NULL, NULL, &timeout); 
}

/* Find out if client has any requests */
int readFromClient(void)
{
    struct timeval timeout;
    fd_set readfds;
    int retval = 0;

    if (clientDead) return 0;
    timeout.tv_sec=0;
    timeout.tv_usec=0;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    if (udpSock >= 0)
	FD_SET(udpSock, &readfds);
    if (select(MAX(sock, udpSock)+1, &readfds, NULL, NULL, &timeout) != 0) {
	/* Read info from the xtrek client */
	if (FD_ISSET(sock, &readfds)) {
	    retval += doRead(sock);
	}
	if (udpSock >= 0 && FD_ISSET(udpSock, &readfds)) {
	    portswapflags |= PORTSWAP_UDPRECEIVED;
	    V_UDPDIAG(("Activity on UDP socket\n"));
	    retval += doRead(udpSock);
	}
    }
    return (retval != 0);               /* convert to 1/0 */
}

static int      rsock;

/* ripped out of above routine */
static int doRead(int asock)
{
    struct timeval timeout;
    fd_set readfds;
    char buf[BUFSIZ*2];
    char *bufptr;
    int size;
    int count;
    int temp;
    struct sockaddr_in moo;
    socklen_t moolen;

    /* need the socket in the ping handler routine */
    rsock = asock;

    /* read the source port of the first UDP packet that comes in, and
       connect() to it -- hack to work with NAT firewalls.

       Note that if PORTSWAP_CONNECTED flag is set, the below 'if' block
       will not be executed.
     */

    if (portswapflags == (PORTSWAP_UDPRECEIVED | PORTSWAP_ENABLED)) {
        moolen = sizeof(moo);
        UDPDIAG(("portswap hack entered\n"));
        if (0 > recvfrom(asock, buf, BUFSIZ*2, MSG_PEEK,
                         (struct sockaddr *)&moo, &moolen)) {
	    ERROR(1,("%s: portswap recvfrom() failed, %s\n",
		     whoami(), strerror(errno)));
        }
        else {
            UDPDIAG(("client port is really %d\n", ntohs(moo.sin_port)));
            portswapflags |= PORTSWAP_CONNECTED;
            portswapflags &= ~PORTSWAP_UDPRECEIVED; 
            if (0 > connect(asock, (struct sockaddr *)&moo, sizeof(moo))) {
		ERROR(1,("%s: portswap connect() failed, %s\n",
			 whoami(), strerror(errno)));
            }
        }
    }

    timeout.tv_sec=0;
    timeout.tv_usec=0;
    FD_ZERO(&readfds);
    FD_SET(asock, &readfds);
    for (;;) {
	count=read(asock,buf,BUFSIZ*2);
	if (count > 0) break;
	if (errno != EINTR) break;
	if (count == 0) errno = 0; /* save reporting bogus errno */
    }
    if (count<=0) {
	/* (this also happens when the client hits 'Q') */
	ERROR(9,("%s: read() failed, %s\n", whoami(), strerror(errno)));
	if (asock == udpSock) {
	    if (errno == ECONNREFUSED) {
		struct sockaddr_in addr;

		UDPDIAG(("Hiccup(%d)!  Reconnecting\n", errno));
		addr.sin_addr.s_addr = remoteaddr;
		addr.sin_port = htons(udpClientPort);
		addr.sin_family = AF_INET;
		if (connect(udpSock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		    ERROR(1,("%s: hiccup connect() failed, %s\n",
			     whoami(), strerror(errno)));
		    UDPDIAG(("Unable to reconnect\n"));
		    /* and fall through to disconnect */
		} else {
		    UDPDIAG(("Reconnect successful\n"));
		    return 0;
		}
	    } /* errno == ECONNREFUSED */

	    UDPDIAG(("*** UDP disconnected (res=%d, err=%d)\n",
		     count, errno));
	    printUdpInfo();
	    closeUdpConn();
	    commMode = COMM_TCP;
#ifdef notdef
	    return 0;  /* I have large questions here -- tell client? */
#endif
	} /* asock == udpSock */
	clientDead=1;
	return 0;
    }
    bufptr=buf;
    while (bufptr < buf+count) {
	if (*bufptr < 1 || *bufptr > NUM_PACKETS || handlers[(int)*bufptr].size==0) {
	    ERROR(1,("%s: unknown packet type: %d, aborting...\n",
		     whoami(), *bufptr));
	    return 0;
	}
	size=handlers[(int)*bufptr].size;
	if(size == -1){     /* variable packet */
	    switch(*bufptr){
	    case CP_S_MESSAGE:
		size =  ((char)bufptr[3]);
		break;
	    default:
		ERROR(1,("Unknown variable packet\n"));
		exit(1);
		break;
	    }
	    if((size % 4) != 0)
		size += (4 - (size % 4));

	}
	while (size>count+(buf-bufptr)) {
	    /* We wait for up to twenty seconds for rest of packet.
	     * If we don't get it, we assume the client died. 
	     */
	    timeout.tv_sec=20;
	    timeout.tv_usec=0;
	    /*readfds=1<<asock;*/
	    FD_ZERO(&readfds);
	    FD_SET(asock, &readfds);
	    if (select(asock+1, &readfds, NULL, NULL, &timeout) == 0) {
		logmessage("Died while waiting for packet...");
		ERROR(1,( "1a) read() failed (%d, error %d)\n",
			  count, errno));
		clientDead=1;
		return 0;
	    }

	    temp=read(asock,buf+count,size-(count+(buf-bufptr)));
	    count+=temp;
	    if (temp<=0) {
		sprintf(buf, "Died in second read(), return=%d", temp);
		logmessage(buf);
		ERROR(1,( "2) read() failed (%d, error %d)\n",
			  count, errno));
		clientDead=1;
		return 0;
	    }
	}
	/* Check to see if the handler is there and the request is legal.
	 * The code is a little ugly, but it isn't too bad to worry about
	 * yet.
	 */
	packetsReceived[(int)*bufptr]++;
	if(asock == udpSock)
	    packets_received++;
	if (handlers[(int)*bufptr].handler != NULL) {
	    if (((FD_ISSET(*bufptr, &inputMask)) &&
		 (me==NULL || (me->p_status == POBSERV) ||
		  !(me->p_flags & (PFWAR|PFREFITTING|PFTWARP)
			  ))) || 
		*bufptr==CP_RESETSTATS || *bufptr==CP_UPDATES ||
		*bufptr==CP_OPTIONS || *bufptr==CP_RESERVED ||
		/* ping response always valid */
		*bufptr==CP_PING_RESPONSE ||
#ifdef RSA            		/* NEW -- fix ghostbust problem */
		*bufptr== CP_RSA_KEY ||
#endif
		*bufptr == CP_FEATURE ||
#ifdef MESSAGES_ALL_TIME	/* off for the moment */
		*bufptr == CP_MESSAGE ||
		*bufptr == CP_S_MESSAGE ||
#endif

		*bufptr==CP_SOCKET || *bufptr==CP_BYE) {
		if (me && me->p_flags & PFSELFDEST
		    && *bufptr != CP_PLANET
                    /* don't let it undo self destruct */
		    && *bufptr != CP_PING_RESPONSE
		    ) {
		    me->p_flags &= ~PFSELFDEST;
		    new_warning(85,"Self Destruct has been canceled");
		}
		(*(handlers[(int)*bufptr].handler))(bufptr);
	    }
	    /* Otherwise we ignore the request */
	} else {
	    ERROR(1,("Handler for packet %d not installed...\n", *bufptr));
	}
	bufptr+=size;
	if (bufptr>buf+BUFSIZ) {
	    memcpy(buf, buf+BUFSIZ, BUFSIZ);
	    if (count==BUFSIZ*2) {
		/*readfds = 1<<asock;*/
		FD_ZERO(&readfds);
		FD_SET(asock, &readfds);
		if (select(asock+1, &readfds, NULL, NULL, &timeout)) {
		    temp=read(asock,buf+BUFSIZ,BUFSIZ);
		    count=BUFSIZ+temp;
		    if (temp<=0) {
			sprintf(buf, "Died in third read(), return=%d", temp);
			ERROR(1,( "3) read() failed (%d, error %d)\n",
				  count, errno));
			logmessage(buf);
			clientDead=1;
			return 0;
		    }
		} else {
		    count=BUFSIZ;
		}
	    } else {
		count -=BUFSIZ;
	    }
	    bufptr-=BUFSIZ;
	}
    }
    return 1;
}

static void handleTorpReq(struct torp_cpacket *packet)
{
    ntorp(packet->dir, TWOBBLE | TOWNERSAFE | TDETTEAMSAFE);
}

static void handlePhasReq(struct phaser_cpacket *packet)
{
    phaser(packet->dir);
}

static void handleSpeedReq(struct speed_cpacket *packet)
{
    set_speed(packet->speed);
}

static void handleDirReq(struct dir_cpacket *packet)
{
    me->p_flags &= ~(PFPLOCK | PFPLLOCK);
    set_course(packet->dir);
}

static void handleShieldReq(struct shield_cpacket *packet)
{
    if (packet->state) {
	shield_up();
    } else {
	shield_down();
    }
}

static void handleRepairReq(struct repair_cpacket *packet)
{
    if (packet->state) {
	repair();
    } else {
	me->p_flags &= ~(PFREPAIR);
    }
}

static void handleOrbitReq(struct orbit_cpacket *packet)
{
    if (packet->state) {
	orbit();
    } else {
	bay_release(me);
	me->p_flags &= ~PFORBIT;
    }
}

static void handlePractrReq(struct practr_cpacket *packet)
{
    if (is_idle(me)) return;
    if (practice_robo()) return;
    if (twarpMode) {
        handleTranswarp();
        return;
    }
}

static void handleBombReq(struct bomb_cpacket *packet)
{
    if (packet->state) {
	bomb_planet();
    } else {
	me->p_flags &= ~(PFBOMB);
    }
}

static void handleBeamReq(struct beam_cpacket *packet)
{
    if (packet->state==1) {
	beam_up();
    } else if (packet->state) {
	beam_down();
    } else {
	me->p_flags &= ~(PFBEAMUP | PFBEAMDOWN);
    }
}

static void handleCloakReq(struct cloak_cpacket *packet)
{
    if (packet->state) {
	cloak_on();
    } else {
	cloak_off();
    }
}

/*ARGSUSED*/
static void handleDetTReq(struct det_torps_cpacket *packet)
{
    if (is_idle(me)) return;
    detothers();
}

/*ARGSUSED*/
static void handleCopilotReq(struct copilot_cpacket *packet)
{
/*
 * Unsupported...
 if (packet->state) {
 me->p_flags |= PFCOPILOT;
 } else {
 me->p_flags &= ~PFCOPILOT;
 }
 */
}

static void handleOutfit(struct outfit_cpacket *packet)
{
    shipPick=packet->ship;
    teamPick=packet->team;
}

static void handleLoginReq(struct login_cpacket *packet)
{
    
    strncpy(namePick, packet->name, NAME_LEN);
    namePick[NAME_LEN-1]=0;
    strncpy(passPick, packet->password, NAME_LEN);
    passPick[NAME_LEN-1]=0;
    /* Is this a name query or a login? */
    if (packet->query) {
	passPick[15]=1;
    }
    strncpy(login, packet->login, NAME_LEN);
    login[NAME_LEN-1]=0;
}

static void handlePlasmaReq(struct plasma_cpacket *packet)
{
    nplasmatorp(packet->dir, 0);
}

static void handleWarReq(struct war_cpacket *packet)
{
    declare_war(packet->newmask, 1);
}

static void handlePlanReq(struct planet_cpacket *packet)
{
    struct planet *plan;
    struct planet_spacket *pl;

    if (!F_check_planets) return;
    if (packet->pnum < 0 || packet->pnum > MAXPLANETS) return;

    plan = &planets[(int) packet->pnum];

    if (plan->pl_info & me->p_team) {
        if ( plan->pl_info != packet->info
             || plan->pl_armies != ntohl(packet->armies)
             || plan->pl_owner != packet->owner
             || (plan->pl_flags & PLFLAGMASK) !=
                (int) (ntohs(packet->flags) & PLFLAGMASK)) {
            pl = calloc(1, sizeof(struct planet_spacket));
            pl->type=SP_PLANET;
            pl->pnum=plan->pl_no;
            pl->info=plan->pl_info;
            pl->flags=htons((short) (plan->pl_flags & PLFLAGMASK));
            pl->armies=htonl(plan->pl_armies);
            pl->owner=plan->pl_owner;
            sendClientPacket(pl);
            free(pl);
            context->cp_planet_miss++;
        } else {
            context->cp_planet_hits++;
        }
    }
}

static void handlePlanlockReq(struct planlock_cpacket *packet)
{
    lock_planet(packet->pnum);
}

static void handlePlaylockReq(struct playlock_cpacket *packet)
{
    lock_player(packet->pnum);
}

static void handleDetMReq(struct det_mytorp_cpacket *packet)
{
    struct torp *t;
    int n = ntohs(packet->tnum);

    if ((n < 0) || (n >= MAXPLAYER*MAXTORP))
	return;
    t = torp_(n);
    if (t->t_owner != me->p_no) 
	return;

    if (send_short) {
	for (t = firstTorpOf(me); t <= lastTorpOf(me); t++) {
	    if (t->t_status == TMOVE) {
		t->t_status = TOFF;
#ifdef LTD_STATS
                /* self detted torp */
                ltd_update_torp_selfdetted(me);
#endif
            }
	}
	return;
    }
    else
	if (t->t_status == TMOVE) {
	    t->t_status = TOFF;
#ifdef LTD_STATS
            /* self detted torp */
            ltd_update_torp_selfdetted(me);
#endif
	}
}

static void handleTractorReq(struct tractor_cpacket *packet)
{
    int target;
    struct player *player;

    if (weaponsallowed[WP_TRACTOR]==0) {
	new_warning(0,"Tractor beams haven't been invented yet.");
	return;
    }
    target=packet->pnum;
    if (packet->state==0) {
	me->p_flags &= ~(PFTRACT | PFPRESS);
	return;
    }
    if (me->p_flags & PFCLOAK) {
	new_warning(1,"Weapons's Officer:  Cannot tractor while cloaked, sir!");
	return;
    }
    if (target<0 || target>=MAXPLAYER || target==me->p_no) return;
    if (me->p_inl_draft != INL_DRAFT_OFF) {
        inl_draft_select(target);
        return;
    }
    player = &players[target];
    if (player->p_flags & PFCLOAK) return;
    if (hypot((double) me->p_x-player->p_x,
	      (double) me->p_y-player->p_y) < 
	((double) TRACTDIST) * me->p_ship.s_tractrng) {
	bay_release(player);
	bay_release(me);
	player->p_flags &= ~PFORBIT;
	me->p_flags &= ~PFORBIT;
	me->p_tractor = target;
	me->p_flags |= PFTRACT;
    } else {
	new_warning(2,"Weapon's Officer:  Vessel is out of range of our tractor beam.");
    }
}

static void handleRepressReq(struct repress_cpacket *packet)
{
    int target;
    struct player *player;

    if (weaponsallowed[WP_TRACTOR]==0) {
	new_warning(0,"Tractor beams haven't been invented yet.");
	return;
    }
    target=packet->pnum;
    if (packet->state==0) {
	me->p_flags &= ~(PFTRACT | PFPRESS);
	return;
    }
    if (me->p_flags & PFCLOAK) {
	new_warning(1,"Weapons's Officer:  Cannot tractor while cloaked, sir!");
	return;
    }
    if (target<0 || target>=MAXPLAYER || target==me->p_no) return;
    if (me->p_inl_draft != INL_DRAFT_OFF) {
        inl_draft_reject(target);
        return;
    }
    player= &players[target];
    if (player->p_flags & PFCLOAK) return;
    if (hypot((double) me->p_x-player->p_x,
	      (double) me->p_y-player->p_y) < 
	((double) TRACTDIST) * me->p_ship.s_tractrng) {
	bay_release(player);
	bay_release(me);
	player->p_flags &= ~PFORBIT;
	me->p_flags &= ~PFORBIT;
	me->p_tractor = target;
	me->p_flags |= (PFTRACT | PFPRESS);
    } else {
	new_warning(2,"Weapon's Officer:  Vessel is out of range of our tractor beam.");
    }
}

static void handleCoupReq(struct coup_cpacket *packet)
{
    coup();

    /* get rid of annoying compiler warning */
    if (packet) return;
}

static void handleRefitReq(struct refit_cpacket *packet)
{
    do_refit(packet->ship);
}

static void handleSMessageReq(struct mesg_s_cpacket *packet)
{
/* If someone would delete the hardcoded things in handleMessageReq */
/*      like     packet->mesg[69]='\0';   */
/* we could give handleMessageReq the packet without copying */
/* But i have no time  HW 04/6/93 */

    struct mesg_cpacket mesPacket;

    mesPacket.type =  CP_MESSAGE;
    mesPacket.group = packet->group;
    mesPacket.indiv  = packet->indiv;
    strncpy(mesPacket.mesg, packet->mesg, MSG_LEN);
    mesPacket.mesg[MSG_LEN - 1] = '\0';
    handleMessageReq(& mesPacket);
/* I hope this was it */
}

static void handleMessageReq(struct mesg_cpacket *packet)
{
    char addrbuf[9];
    static time_t lasttime=0 /*, time()*/ ;
    static int balance=0;	/* make sure he doesn't get carried away */
    time_t thistime;
    int group=0;

    if (mute || (observer_muting && (me->p_status == POBSERV)) || (muteall && (packet->group & MALL))) {
        new_warning(86, "Be quiet");
        return;
    }

    /* Some random code to make sure the player doesn't get carried away 
     *  about the number of messages he sends.  After all, he could try to 
     *  jam peoples communications if we let him.
     */
    thistime=time(NULL);
    if (lasttime!=0) {
	balance=balance-(thistime-lasttime);
	if (balance<0) balance=0;
    }
    lasttime=thistime;
    if (balance>=15 && !whitelisted) {
	new_warning(86,"Be quiet");
	balance+=3;
	if (status->tourn)
	    balance+=3;
	return;
    }
    if (!inl_mode || !me->p_inl_captain)
        balance+=3;
#ifdef CONTINUUM
    if (me->p_status == POBSERV) {
	if (!whitelisted) balance+=20;
    }
#endif
    packet->mesg[MSG_LEN - 11]='\0';	/* 11 = 10(why?) - 1 for 0 placement */
    sprintf(addrbuf, " %c%c->", teamlet[me->p_team], shipnos[me->p_no]);
    if (parseIgnore(packet)) return; /* moved this up 4/6/92 TC */

    /* Here is where we handle special client macros. This lets us use more 
       bits in group! Yah! 	   */
    if (packet->group & FMMACRO) {
	if (!(packet->group & 0x40)) {		/* 0x40 + FMMACRO = MDISTR */
	    packet->group &= ~FMMACRO;
	    group = MMACRO;
	}
    }
    switch(packet->group) {
    case MALL:
	sprintf(addrbuf+5, "ALL");
	break;
    case (MTEAM|MDISTR):
    case MTEAM:
	if (packet->indiv != FED && packet->indiv != ROM &&
	    packet->indiv != KLI && packet->indiv != ORI) return;
	sprintf(addrbuf+5, teamshort[packet->indiv]);
	clueFuse = 0;
	break;
    case MINDIV:
	/* packet->indiv is of type unsigned char, < 0 is always false -da */
	/* if (packet->indiv < 0 || packet->indiv >= MAXPLAYER) return; */
	if (packet->indiv >= MAXPLAYER) return;

	if (players[packet->indiv].p_status == PFREE) return;
	if (ignored[packet->indiv] & MINDIV) {
	    new_warning(88,"You are ignoring that player.  Message was not sent.");
	    return;
	}
#ifdef STURGEON
        if (sturgeon) {
            struct player *dude;
            char buf2[20];
            int i;

            dude = &players[packet->indiv];
            if ((packet->indiv == me->p_no) & (strlen(packet->mesg) == 1)) {
                switch (toupper(*packet->mesg)) {
                    case 'C':
                        sprintf(addrbuf, "GOD->%c%c ", teamlet[me->p_team], shipnos[me->p_no]);
                        pmessage(me->p_no, MINDIV, addrbuf,"[C] this message, [u] list upgrades, [k] list kills, [i] inventory");
                        sprintf(addrbuf, "GOD->%c%c ", teamlet[me->p_team], shipnos[me->p_no]);
                        sprintf(buf, "Lite: %s. Extrakills: %s. Planetupgrades: %s. Maxupgrades: ",
                                sturgeon_lite ? "on" : "off",
                                sturgeon_extrakills ? "on" : "off",
                                sturgeon_planetupgrades ? "on" : "off");
                        if (sturgeon_maxupgrades)
                            sprintf(buf2, "%d", sturgeon_maxupgrades);
                        else
                            sprintf(buf2, "inf");
                        strcat(buf, buf2);
                        pmessage(me->p_no, MINDIV, addrbuf, buf);
                        sprintf(buf, "Special Weapons: %s", 
                                sturgeon_special_weapons ? "on" : "off");
                        pmessage(me->p_no, MINDIV, addrbuf, buf);
                        return;
                }
            }
            if (!strcmp(packet->mesg, "i") || !strcmp(packet->mesg, "I")) {
                if ((me->p_special < 0) && (packet->indiv == me->p_no)) {
                    new_warning(UNDEF,"Our ship has no special weapons");
                    return;
                }
                if (packet->indiv == me->p_no) {
                    sprintf(addrbuf, "UPG->%c%c ", teamlet[me->p_team], shipnos[me->p_no]);
                    pmessage(me->p_no, MINDIV, addrbuf,"Inventory:");
                }
                else if (dude->p_team == me->p_team) {
                    sprintf(addrbuf, " %c%c->%c%c ", teamlet[dude->p_team], shipnos[dude->p_no], teamlet[me->p_team], shipnos[me->p_no]);
                    pmessage(me->p_no, MINDIV, addrbuf,"Our inventory:");
                    if (dude->p_special < 0) {
                        pmessage(me->p_no, MINDIV, addrbuf,"None.");
                        return;
                    }
                }
                else {
                    sprintf(addrbuf, "SPY->%c%c ", teamlet[me->p_team], shipnos[me->p_no]);
                    sprintf(buf, "Intelligence indicates that %c%c's inventory includes:", teamlet[dude->p_team], shipnos[dude->p_no]);
                    pmessage(me->p_no, MINDIV, addrbuf,buf);
                }
                if (dude->p_team == me->p_team) {
                    for (i=0; i < NUMSPECIAL; i++) {
                        if (dude->p_weapons[i].sw_number < 0)
                            sprintf(buf, " inf %s", dude->p_weapons[i].sw_name);
                        else if (dude->p_weapons[i].sw_number > 0)
                            sprintf(buf, "%4d %s", dude->p_weapons[i].sw_number, dude->p_weapons[i].sw_name);
                        else
                            continue;
                        pmessage(me->p_no, MINDIV, addrbuf,buf);
                    }
                }
                else {
                    *buf = '\0';
                    for (i=0; i<=5; i++)
                    if (dude->p_weapons[i].sw_number) {
                        strcat(buf, "plasmas ");
                        break;
                    }
                    for (i=6; i <= 9; i++) {
                        if (dude->p_weapons[i].sw_number) {
                            strcat(buf, "nukes ");
                            break;
                        }
                    }
                    if (dude->p_weapons[10].sw_number)
                        strcat(buf, "drones ");
                    if (dude->p_weapons[11].sw_number)
                        strcat(buf, "mines ");

                    if (*buf)
                        pmessage(me->p_no, MINDIV, addrbuf,buf);
                    else
                        pmessage(me->p_no, MINDIV, addrbuf,"No special weapons");
                }
                return;
            }
            if (!strcmp(packet->mesg, "u") || !strcmp(packet->mesg, "U")) {
                if ((me->p_upgrades == 0.0) && (dude->p_no == me->p_no)) {
                    new_warning(UNDEF,"Our ship has no upgrades");
                    return;
                }
                if (packet->indiv == me->p_no) {
                    sprintf(addrbuf, "UPG->%c%c ", teamlet[me->p_team], shipnos[me->p_no]);
                    pmessage(me->p_no, MINDIV, addrbuf,"Current upgrades (cost for next):");
                }
                else if (dude->p_team == me->p_team) {
                    sprintf(addrbuf, " %c%c->%c%c ",
                    teamlet[dude->p_team], shipnos[dude->p_no],
                    teamlet[me->p_team], shipnos[me->p_no]);
                    pmessage(me->p_no, MINDIV, addrbuf,"Our ship's current upgrades:");
                    if (dude->p_upgrades == 0.0) {
                        pmessage(me->p_no, MINDIV, addrbuf,"None.");
                        return;
                    }
                }
                else {
                    sprintf(addrbuf, "SPY->%c%c ", teamlet[me->p_team], shipnos[me->p_no]);
                    sprintf(buf, "%c%c has spent %.2f kills upgrading the following systems:", teamlet[dude->p_team], shipnos[dude->p_no], dude->p_upgrades);
                    pmessage(me->p_no, MINDIV, addrbuf,buf);
                }
                if (me->p_team == dude->p_team) {
                    for (i=0; i < NUMUPGRADES; i++) {
                        if (dude->p_upgradelist[i] > 0) {
                            if (i < UPG_OFFSET) {
                                if (dude->p_no == me->p_no)
                                    sprintf(buf, "%4d (%6.2f) %s", dude->p_upgradelist[i], baseupgradecost[i] + dude->p_upgradelist[i] * adderupgradecost[i], upgradename[i]);
                                else
                                    sprintf(buf, "%4d %s", dude->p_upgradelist[i], upgradename[i]);
                            }
                            else
                                sprintf(buf, "%s upgrade", upgradename[i]);
                            pmessage(me->p_no, MINDIV, addrbuf, buf);
                        }
                    }
                }
                else {
                    *buf = '\0';
                    if (dude->p_upgradelist[UPG_PERMSHIELD])
                        strcat(buf, "shields ");
                    if (dude->p_upgradelist[UPG_HULL])
                        strcat(buf, "hull ");
                    if (dude->p_upgradelist[UPG_FUEL] || dude->p_upgradelist[UPG_RECHARGE]) {
                        strcat(buf, "fuel ");
                    }
                    if (dude->p_upgradelist[UPG_MAXWARP] || dude->p_upgradelist[UPG_ACCEL]
                     || dude->p_upgradelist[UPG_DECEL] || dude->p_upgradelist[UPG_ENGCOOL]) {
                        strcat(buf, "engines ");
                    }
                    if (dude->p_upgradelist[UPG_PHASER] || dude->p_upgradelist[UPG_TORPSPEED]
                     || dude->p_upgradelist[UPG_TORPFUSE] || dude->p_upgradelist[UPG_WPNCOOL]) {
                        strcat(buf, "weapons ");
                    }
                    if (dude->p_upgradelist[UPG_CLOAK])
                        strcat(buf, "cloak ");
                    if (dude->p_upgradelist[UPG_TPSTR] || dude->p_upgradelist[UPG_TPRANGE]) {
                        strcat(buf, "tractors ");
                    }
                    if (dude->p_upgradelist[UPG_REPAIR])
                        strcat(buf, "repair ");
                    if (dude->p_upgradelist[UPG_FIRECLOAK])
                        strcat(buf, "fire-while-cloaked ");
                    if (dude->p_upgradelist[UPG_DETDMG])
                        strcat(buf, "det-own-torps-for-damage ");
                    if (*buf != '\0')
                        pmessage(me->p_no, MINDIV, addrbuf,buf);
                    else
                        pmessage(me->p_no, MINDIV, addrbuf,"none");
                }
                return;
            }
            if (!strcmp(packet->mesg, "k") || !strcmp(packet->mesg, "K")) {
                if (dude->p_team == me->p_team) {
                    if (dude->p_ship.s_type == ASSAULT)
                        i = dude->p_kills * 3.0;
                    else if (dude->p_ship.s_type == STARBASE)
                        i = 25;
                    else
                        i = dude->p_kills * 2.0;
                    if (i > dude->p_ship.s_maxarmies)
                        i = dude->p_ship.s_maxarmies;
                    sprintf(buf,"%.2f kills/rank credit available (%.2f total), %d/%d armies",
                            dude->p_kills + dude->p_rankcredit, dude->p_kills + dude->p_rankcredit + dude->p_upgrades,
                            dude->p_armies, i);
                    new_warning(UNDEF,buf);
                }
                else {
                    new_warning(UNDEF,"Kill information only available from teammates");
                }
                return;
            }
        }
#endif
	sprintf(addrbuf+5, "%c%c ", teamlet[players[packet->indiv].p_team],
		shipnos[packet->indiv]);
	break;
    case (MINDIV|MCONFIG):
	clientVersion((struct mesg_spacket *) packet);
	break;
    case MGOD:
	sprintf(addrbuf+5, "GOD");
	break;
    default:
	return;
    }
    group |= packet->group;
    pmessage2(packet->indiv, group, addrbuf, me->p_no, "%s", packet->mesg);
    if (checkmessage) {
        if (check_mesgs(packet))
            return;
    }
    if ((clue) && (!clueVerified))
	check_clue(packet);
}

/*ARGSUSED*/
static void handleQuitReq(struct quit_cpacket *packet)
{
    int sd_time = (me->p_ship.s_type != STARBASE) ? 10 : 60;
    if (is_idle(me)) sd_time = sd_time / 5;
    me->p_selfdest = me->p_updates + sd_time * 10;
    me->p_flags |= PFSELFDEST;
    new_warning(90,"Self destruct initiated");
}

static void handleOptionsPacket(struct options_cpacket *packet)
{
    memcpy(mystats->st_keymap, packet->keymap, 96);
/*    mystats->st_flags = ntohl(packet->flags);*/
    mystats->st_flags = ntohl(packet->flags) |
	(mystats->st_flags & ST_CYBORG); /* hacked fix 8/24/91 TC */
    keeppeace=(mystats->st_flags / ST_KEEPPEACE) & 1;
}

static void handleSocketReq(struct socket_cpacket *packet)
{
    nextSocket=ntohl(packet->socket);
    userVersion=packet->version;
    userUdpVersion=packet->udp_version;
}

/*ARGSUSED*/
static void handleByeReq(struct bye_cpacket *packet)
{
}

int checkVersion(void)
{
    struct badversion_spacket packet;

    if (userVersion != SOCKVERSION) {
	memset(&packet, 0, sizeof(struct badversion_spacket)); /* bzero LEGACY POSIX.1-2001 */
	packet.type = SP_BADVERSION;
	packet.why = BADVERSION_SOCKET;
	sendClientPacket((CVOID) &packet);
	flushSockBuf();
	return 0;
    }
    return 1;
}

void logEntry(void)
{
    FILE *logfile;
    time_t curtime;

    logfile=fopen(LogFileName, "a");
    if (!logfile) return;
    curtime=time(NULL);
    fprintf(logfile, "Joining: %s, (%c) %s\t<%s@%s>\n", me->p_name,
	    shipnos[me->p_no],
	    ctime(&curtime),
	    me->p_login, /* debug 2/21/92 TMC */
	    me->p_full_hostname
	);
    fclose(logfile);
}

static int gwrite(int fd, char *wbuf, size_t size)
{
    size_t orig = size;
    register int n;
    char tempbuf[MSG_LEN];
    register size_t bytes = size;
    register int count = 0;

    if (clientDead) return 0;

    if (bytes>BUFSIZE) {
	ERROR(1,("ERROR!!! gwrite got passed buf size of %d\n", (int)bytes));
	return -1;
    }

    while (bytes>0) {
	n = write(fd, wbuf, bytes);
	if (count++ > 100) {
	    ERROR(1,("Gwrite hosed: too many writes (%d)\n",getpid()));
	    clientDead = 1;
	    return -1;
	}
	if (n < 0) {
	    if (errno==ENOBUFS) {
		/* The man pages don't mention this as a possibility.
		 * Yet, it happens.  I guess I just wait for 1/10 sec, and
		 *  continue?
		 */
		usleep(100000);
		errno = (-1);		/* paranoia */
		continue;
	    }
	    else if (errno==EINTR) {
		/* We got interrupted... Let's try it again. */
		perror("gwrite");
		errno = (-1);
		n = 0;
	    }
	    else if (errno==EPIPE) {
		/* The pipe is broken: i.e. the client is dead */
		/* clientDead is marked outside of gwrite */
		return -1;
	    }
	    else {
	    	perror("gwrite");
		return -1;
	    }
	    if (fd == udpSock) {
		/* do we want Hiccup code here? */
		UDPDIAG(("write() failed, fd=%d, bytes=%d, errno=%d\n",
			 fd, (int)bytes, errno));
		printUdpInfo();
		logmessage("UDP gwrite failed:");
	    }
	    sprintf(tempbuf, "Died in gwrite, n=%d, errno=%d <%s@%s>",
		    n, errno, me->p_login, me->p_full_hostname);
	    logmessage(tempbuf);
	    return -1;
	}
	bytes -= n;
	wbuf += n;
    }
    return orig;
}

static void handleDockingReq(struct dockperm_cpacket *packet)
{
    if (me->p_ship.s_type == STARBASE) {
        if (packet->state) {
            me->p_flags |= PFDOCKOK;
        } else {
            me->p_flags &= ~PFDOCKOK;
            bay_release_all(me);
        }
    }
}

static void handleReset(struct resetstats_cpacket *packet)
{
#ifndef lint
    extern int startTkills, startTlosses, startTarms, 
	startTplanets, startTticks;
#endif
    if (packet->verify != 'Y') return;

    /* Gee, they seem to want to reset their stats!  Here goes... 
     */

#ifdef LTD_STATS

    ltd_reset(me);
    mystats->st_rank=0;

#else
    mystats->st_maxkills=0.0;
    mystats->st_kills=0;
    mystats->st_losses=0;
    mystats->st_armsbomb=0;
    mystats->st_planets=0;
    mystats->st_ticks=0;
    mystats->st_tkills=0;
    mystats->st_tlosses=0;
    mystats->st_tarmsbomb=0;
    mystats->st_tplanets=0;
    mystats->st_tticks=1;
    mystats->st_rank=0;
    mystats->st_sbkills=0;
    mystats->st_sblosses=0;
    mystats->st_sbticks=0;
    mystats->st_sbmaxkills=0.0;

#endif /* LTD_STATS */

#ifdef GENO_COUNT
    mystats->st_genos=0;
#endif

#ifndef lint

#ifdef LTD_STATS

    startTkills   = ltd_kills(me, LTD_TOTAL);
    startTlosses  = ltd_deaths(me, LTD_TOTAL);
    startTarms    = ltd_armies_bombed(me, LTD_TOTAL);
    startTplanets = ltd_planets_taken(me, LTD_TOTAL);
    startTticks   = ltd_ticks(me, LTD_TOTAL);

    startSBkills  = ltd_kills(me, LTD_SB);
    startSBlosses = ltd_deaths(me, LTD_SB);
    startSBticks  = ltd_ticks(me, LTD_SB);

#else

    startTkills = mystats->st_tkills;
    startTlosses = mystats->st_tlosses;
    startTarms = mystats->st_tarmsbomb;
    startTplanets = mystats->st_tplanets;
    startTticks = mystats->st_tticks;

#endif /* LTD_STATS */
#endif /* lint */
}

static void handleUpdatesReq(struct updates_cpacket *packet)
{
    /* the client sends how often he would like to receive a packet in
    microseconds ... which is converted to an integer rate in updates
    per second */
    int ups = 1000000 / ntohl(packet->usecs);
    if (p_ups_set(me, ups)) {
        /* FIXME: only send this if the client has shown it
        understands feature packets, by sending us CP_FEATURE */
        sendFeatureUps();
    }
}

static void logmessage(char *string)
{
    FILE *fp;

    fp=fopen(LogFileName, "a");
    if (fp) {
	fprintf(fp, "%s\n",string);
	fclose(fp);
    }
}

static void clientVersion(struct mesg_spacket *packet)
{
    char *mesg = packet->mesg;
    if (*mesg == '@') {
        mesg++;
        strncpy(me->p_ident, mesg, 80);
        ERROR(1,("%s: ident %s\n", whoami(), mesg));
    }
}

#ifdef RSA

static void handleReserved(struct reserved_cpacket *packet)
{
    struct reserved_cpacket mycp;
    struct reserved_spacket mysp;
    struct rsa_key_spacket   rsp;
    char serverName[64];

    if (testtime==1) return;
    if (memcmp(packet->data, testdata, RESERVED_SIZE) != 0) { /* bcmp LEGACY POSIX.1-2001 */
        testtime=1;
        return;
    }
    if (!strncmp (packet->resp, RSA_VERSION, 3)) {
	/* This is an RSA type client */
	RSA_Client = 2;
	new_warning (UNDEF,"%s",RSA_VERSION);
	if (!strncmp (packet->resp, RSA_VERSION, strlen("RSA v??"))) {
	    /* This is the right major version */
	    RSA_Client = 1;
	    makeRSAPacket(&rsp);
	    memcpy(testdata, rsp.data, KEY_SIZE);
	    sendClientPacket (&rsp);
	    return;
	}
	testtime=1;
	return;
    }
    memcpy(mysp.data, testdata, RESERVED_SIZE); /* bcopy LEGACY POSIX.1-2001 */
    serverName[0] = '\0';
    if (gethostname(serverName, 64))
	ERROR(1,( "%s: gethostname() failed with %s", whoami(), 
		  strerror(errno)));
    encryptReservedPacket(&mysp, &mycp, serverName, me->p_no);
    if (memcmp(packet->resp, mycp.resp, RESERVED_SIZE) != 0) {
        ERROR(3, ("%s: user verified incorrectly.\n", whoami()));
        testtime=1;
        return;
    }
    /* Use .sysdef CONFIRM flag to allow old style clients. */
    if (binconfirm==2)
        testtime=0;
    else
        testtime=1;
}

static void handleRSAKey(struct rsa_key_cpacket *packet)
{
    struct rsa_key_spacket mysp;
    char serverName[64]; 

    if (testtime==1) return;
    if (RSA_Client != 1) return;
    memcpy(mysp.data, testdata, KEY_SIZE);

    serverName[0] = '\0';
    if (gethostname(serverName, 64))
	ERROR(1,( "%s: gethostname() failed with %s\n", whoami(), 
		  strerror(errno)));
    if (decryptRSAPacket(&mysp, packet, serverName))
    {
        ERROR(3,("%s: user verified incorrectly.\n", whoami()));
        testtime=1;
        return;
    }
    testtime=0;
}


#else

static void handleReserved(struct reserved_cpacket *packet)
{
    struct reserved_cpacket mycp;
    struct reserved_spacket mysp;
    char serverName[64];

    if (testtime==1) return;
    if (memcmp(packet->data, testdata, 16) != 0) {
	testtime=1;
	return;
    }
    memcpy(mysp.data, testdata, 16);
    serverName[0] = '\0';
    if (gethostname(serverName, 64))
	ERROR(1,( "%s: gethostname() failed, %s\n", whoami(), 
		  strerror(errno)));
    encryptReservedPacket(&mysp, &mycp, serverName, me->p_no);
    if (memcmp(packet->resp, mycp.resp, 16) != 0) {
        ERROR(3,("%s: user verified incorrectly.\n", whoami()));
	testtime=1;
	return;
    }
    testtime=0;
}
#endif


static void handleScan(struct scan_cpacket *packet)
{
/*
  struct scan_spacket response;
  struct player *pp;

  memset(&response, 0, sizeof(struct scan_spacket));
  response.type = SP_SCAN;
  response.pnum = packet->pnum;
  if (!weaponsallowed[WP_SCANNER]) {
  new_warning(91,"Scanners haven't been invented yet");
  response.success = 0;
  } else {
  response.success = scan(packet->pnum);

  if (response.success) {
  / fill in all the goodies /
  pp = &players[packet->pnum];
  response.p_fuel = htonl(pp->p_fuel);
  response.p_armies = htonl(pp->p_armies);
  response.p_shield = htonl(pp->p_shield);
  response.p_damage = htonl(pp->p_damage);
  response.p_etemp = htonl(pp->p_etemp);
  response.p_wtemp = htonl(pp->p_wtemp);
  }
  }
  sendClientPacket((CVOID) &response);
  */

  /* get rid of annoying compiler warning */
  if (packet) return;
}

static void handlePingResponse(struct ping_cpacket  *packet)
{
    /* client requests pings by sending pingme == 1 on TCP socket */

    if(rsock == sock){
	if(!ping && packet->pingme == 1){
	    ping = 1;
#if PING_DEBUG >= 1
	    ERROR(1,( "ping on for %s@%s\n", me->p_name, me->p_monitor));
	    fflush(stderr);
#endif
	    if (ping_freq)
		new_warning(UNDEF, "Server sending PING packets at %d second intervals", ping_freq);
	    else 
		new_warning(UNDEF, "Server sending PING packets on every update.");
	    return;
	}
	/* client says stop */
	else if(ping && !packet->pingme){
	    ping = 0;
#if PING_DEBUG >= 1
	    ERROR(1,( "ping off for %s@%s\n", me->p_name, me->p_monitor));
	    fflush(stderr);
#endif
	    new_warning(UNDEF,"Server no longer sending PING packets.");
	    return;
	}
    }
    pingResponse(packet);        /* ping.c */
}

#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
/* these are sent by the robots when a parameter changes */
static void handleOggV(struct oggv_cpacket *packet)
{
    me->p_df = packet->def;
    me->p_tg = packet->targ;

    if (me->p_df > 100)
	me->p_df = 100;

    me->p_flags |= PFBPROBOT;

#ifdef DEBUG
    printf("%c%c (%s) %d, targ: %d\n",
	   me->p_mapchars[0], me->p_mapchars[1],
	   me->p_name, me->p_df, me->p_tg - 1);
    fflush(stdout);
#endif
}
#endif                          /* BASEPRACTICE || NEWBIESERVER */

static void handleFeature(struct feature_cpacket *cpacket)
{
    struct feature_spacket       spacket;
    int was = F_client_feature_packets;

    F_client_feature_packets = 1;        /* XX */

    cpacket->value = ntohl(cpacket->value);
    memset(&spacket, 0, sizeof(struct feature_spacket));
    getFeature(cpacket, &spacket);
    sendFeature(&spacket);
    if (!was) { /* initial feature packets to be sent by server */
        sendFeatureFps();
        sendFeatureUps();
        sendLameRefit();
        sendLameBaseRefit();
    }
}

/*
 * ---------------------------------------------------------------------------
 *	Strictly UDP from here on
 * ---------------------------------------------------------------------------
 */

static void handleUdpReq(struct udp_req_cpacket *packet)
{
    struct udp_reply_spacket response;
    int mode;

    response.type = SP_UDP_REPLY;

    if (packet->request == COMM_VERIFY) {
	/* this request should ONLY come through the UDP connection */
	if (commMode == COMM_UDP) {
	    UDPDIAG(("Got second verify from %s; resending server verify\n",
		     me->p_name));
	    response.reply = SWITCH_VERIFY;
	    goto send;
	}
	UDPDIAG(("Receieved UDP verify from %s\n", me->p_name));
	UDPDIAG(("--- UDP connection established to %s\n", me->p_name));
#ifdef BROKEN
        new_warning(92,"WARNING: BROKEN mode is enabled");
#endif

	sequence = 1;		/* reset sequence numbers */
	commMode = COMM_UDP;	/* at last */
	udpMode = MODE_SIMPLE;	/* just send one at a time */
		
	/* note that we don't NEED to send a SWITCH_VERIFY packet; the client */
	/* will change state when it receives ANY packet on the UDP connection*/
	/* (this just makes sure that it gets one) */
	/* (update: recvfrom() currently tosses the first packet it gets...)  */
	response.reply = SWITCH_VERIFY;
	goto send;
/*	return;*/
    }

    if (packet->request == COMM_MODE) {
	/* wants to switch modes; mode is in "conmode" */
	mode = packet->connmode;
	if (mode < MODE_TCP || mode > MODE_DOUBLE) {
            new_warning(93,"Server can't do that UDP mode");
	    UDPDIAG(("Got bogus request for UDP mode %d from %s\n",
		     mode, me->p_name));
	} else {
	    /* I don't bother with a reply, though it can mess up the opt win */
	    switch (mode) {
	    case MODE_TCP:
                new_warning(94,"Server will send with TCP only");
		break;
	    case MODE_SIMPLE:
                new_warning(95,"Server will send with simple UDP");
		break;
	    case MODE_FAT:
		new_warning(UNDEF,"Server will send with fat UDP; sent full update");
		V_UDPDIAG(("Sending full update to %s\n", me->p_name));
		forceUpdate();
		break;
#ifdef DOUBLE_UDP
	    case MODE_DOUBLE:
		if(send_short){
		    swarning(TEXTE,96,0);
		    mode = MODE_SIMPLE;
		    break;
		}
		new_warning(UNDEF,"Server will send with double UDP");
		scbufptr=scbuf + sizeof(struct sc_sequence_spacket);
		break;
#else
	    case MODE_DOUBLE:
                new_warning(97,"Request for double UDP DENIED (set to simple)");
		mode = MODE_SIMPLE;
		break;
#endif /*DOUBLE_UDP*/
	    }

	    udpMode = mode;
	    UDPDIAG(("Switching %s to UDP mode %d\n", me->p_name, mode));
	}
	return;
    }

    if (packet->request == COMM_UPDATE) {
	/* client wants a FULL update */
	V_UDPDIAG(("Sending full update to %s\n", me->p_name));
	forceUpdate();

	return;
    }

    UDPDIAG(("Received request for %s mode from %s\n",
	     (packet->request == COMM_TCP) ? "TCP" : "UDP", me->p_name));
    if (packet->request == commMode) {
	/* client asking to switch to current mode */
	if (commMode == COMM_UDP) {
	    /*
	     * client must be confused... whatever the cause, he obviously
	     * isn't connected to us, so we better drop out end and retry.
	     */
	    UDPDIAG(("Rcvd UDP req from %s while in UDP mode; dropping old\n",
		     me->p_name));
	    closeUdpConn();
	    commMode = COMM_TCP;
	    /* ...and fall thru to the UDP request handler */
	} else {
	    /*
	     * Again, client is confused.  This time there's no damage though.
	     * Just tell him that he succeeded.  Could also happen if the
	     * client tried to connect to our UDP socket but failed, and
	     * decided to back off.
	     */
	    UDPDIAG(("Rcvd TCP req from %s while in TCP mode\n", me->p_name));

	    response.reply = SWITCH_TCP_OK;
	    sendClientPacket((CVOID) &response);

	    if (udpSock >= 0) {
		closeUdpConn();
		UDPDIAG(("Closed UDP socket\n"));
	    }
	    return;
	}
    }

    /* okay, we have a request to change modes */
    if (packet->request == COMM_UDP) {
	udpClientPort = ntohl(packet->port);	/* where to connect to */
	if (!udpAllowed) {
	    UDPDIAG(("Rejected UDP request from %s\n", me->p_name));
	    response.reply = SWITCH_DENIED;
	    response.port = htons(0);
	    goto send;
	} else {
	    if (userUdpVersion != UDPVERSION) {
		new_warning(UNDEF, "Server UDP is v%.1f, client is v%.1f",
			    (float) UDPVERSION / 10.0,
			    (float) userUdpVersion / 10.0);
		UDPDIAG(("Server UDP (rejected %s)\n", me->p_name));
		response.reply = SWITCH_DENIED;
		response.port = htons(1);
		goto send;
	    }
	    if (udpSock >= 0) {
		/* we have a socket open, but the client doesn't seem aware */
		/* (probably because our UDP verify got lost down the line) */
		UDPDIAG(("Receieved second request from %s, reconnecting\n",
			 me->p_name));
		closeUdpConn();
	    }
            if (packet->connmode == CONNMODE_PORT) {
                portswapflags |= PORTSWAP_ENABLED;
            }
	    /* (note no openUdpConn(); we go straight to connect) */
	    if (connUdpConn() < 0) {
		response.reply = SWITCH_DENIED;
		response.port = 0;
		goto send;
	    }
	    UDPDIAG(("Connected UDP socket (%d:%d) for %s\n", udpSock,
		     udpLocalPort, me->p_name));

	    /* we are now connected to the client, but he's merely bound */
	    /* don't switch to UDP mode yet; wait until client connects */
	    response.reply = SWITCH_UDP_OK;
	    response.port = htonl(udpLocalPort);

	    UDPDIAG(("packet->connmode = %d\n", packet->connmode));
	    if (packet->connmode == CONNMODE_PORT) {
		/* send him our port # so he can connect to us */
		goto send;
	    } else {
		/* send him a packet; he'll get port from recvfrom() */
		if (gwrite(udpSock, (char *) &response, sizeof(response)) !=
		    sizeof(response)) {
		    perror("send UDP packet failed, client not marked dead");
		    UDPDIAG(("Attempt to send UDP packet failed; using alt\n"));
		}
		goto send;
	    }
	}
    } else if (packet->request == COMM_TCP) {
	closeUdpConn();
	commMode = COMM_TCP;
	response.reply = SWITCH_TCP_OK;
	response.port = 0;
	UDPDIAG(("Closed UDP socket for %s\n", me->p_name));
	goto send;
    } else {
	ERROR(1,( "ntserv: got weird UDP request (%d)\n",
		  packet->request));
	return;
    }
send:
    sendClientPacket((CVOID) &response);
}


static int connUdpConn(void)
{
    struct sockaddr_in addr;
    socklen_t addrlen;

    if (udpSock > 0) {
	ERROR(2,( "ntserv: tried to open udpSock twice\n"));
	return 0;	/* pretend we succeeded (this could be bad) */
    }
    if (udpbufptr != udpbuf) {
	udpbufptr = udpbuf;		/* clear out any old data */
	sequence--;			/* we just killed a sequence packet */
    }

    if ((udpSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("ntserv: unable to create DGRAM socket");
	return -1;
    }

#ifdef UDP_FIX 			/* 15/6/93 SK UDP connection time out fix */
    /* Bind to interface used by the TCP socket 10/13/99 TAP */
    addrlen = sizeof(addr);
    if (getsockname(sock, (struct sockaddr *)&addr, &addrlen) < 0) {
	perror("netrek: unable to getsockname(TCP)");
	UDPDIAG(("Can't get our own socket; using default interface\n"));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
    } else {
	UDPDIAG(("Using interface 0x%x\n", ntohl(addr.sin_addr.s_addr)));
    }

    if (bind_udp_port_base == 0) {
        addr.sin_port = 0;
    } else {
        addr.sin_port = htons(bind_udp_port_base + me->p_no);
    }

    if (bind(udpSock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("ntserv: cannot bind to local port");
        close(udpSock);
	udpSock = -1;
	return -1;
    }

    /* determine what our port is */
    addrlen = sizeof(addr);
    if (getsockname(udpSock, (struct sockaddr *)&addr, &addrlen) < 0) {
	perror("netrek: unable to getsockname(UDP)");
	UDPDIAG(("Can't get our own socket; connection failed\n"));
	close(udpSock);
	udpSock = -1;
	return -1;
    }
    udpLocalPort = (int) ntohs(addr.sin_port);

    caddr.sin_family = AF_INET;
    caddr.sin_addr.s_addr = remoteaddr; /* addr of our client */
    caddr.sin_port = htons(udpClientPort);      /* client's port */

    UDPDIAG(("UDP_FIX code enabled.  portswapflags = %d\n", portswapflags));

    if (portswapflags & PORTSWAP_ENABLED) {
        UDPDIAG(("portswap mode -- putting of connect() until later\n"));
    }
    else
    if (connect(udpSock, (struct sockaddr *) &caddr, sizeof(caddr)) < 0)
#else /* !UDP_FIX */

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = remoteaddr;	/* addr of our client */
    addr.sin_port = htons(udpClientPort);	/* client's port */

    UDPDIAG(("UDP_FIX code disabled.  portswapflags = %d\n", portswapflags));

    if (portswapflags & PORTSWAP_ENABLED) {
        UDPDIAG(("portswap mode -- putting of connect() until later\n"));
    }
    else
    if (connect(udpSock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
#endif /* UDP_FIX */
    {
	perror("ntserv: connect to client UDP port");
	UDPDIAG(("Unable to connect() to %s on port %d\n", me->p_name,
		 udpClientPort));
	close(udpSock);
	udpSock = -1;
	return -1;
    }
    UDPDIAG(("connect to %s's port %d on 0x%x succeded\n",
	     me->p_name, udpClientPort, remoteaddr));

    /* determine what our port is */
    addrlen = sizeof(addr);
    if (getsockname(udpSock, (struct sockaddr *) &addr, &addrlen) < 0) {
	perror("netrek: unable to getsockname(UDP)");
	UDPDIAG(("Can't get our own socket; connection failed\n"));
	close(udpSock);
	udpSock = -1;
	return -1;
    }
    udpLocalPort = (int) ntohs(addr.sin_port);

    if (udpAllowed > 2)		/* verbose debug mode? */
	printUdpInfo();

    return 0;
}

int closeUdpConn(void)
{
    V_UDPDIAG(("Closing UDP socket\n"));
    if (udpSock < 0) {
	ERROR(2,( "ntserv: tried to close a closed UDP socket\n"));
	return -1;
    }
    shutdown(udpSock, 2);	/* wham */
    close(udpSock);		/* bam */
    udpSock = -1;		/* (nah) */

    portswapflags &= ~(PORTSWAP_CONNECTED | PORTSWAP_ENABLED);
    UDPDIAG(("Disabling PORTSWAP mode.  Flags = %d\n", portswapflags));
    return 0;
}

/* used for debugging */
static void printUdpInfo(void)
{
    struct sockaddr_in addr;
    socklen_t addrlen;

    addrlen = sizeof(addr);
    if (getsockname(udpSock, (struct sockaddr *) &addr, &addrlen) < 0) {
	perror("printUdpInfo: getsockname");
	return;
    }
    UDPDIAG(("LOCAL: addr=0x%x, family=%d, port=%d\n", addr.sin_addr.s_addr,
	     addr.sin_family, ntohs(addr.sin_port)));

    if (getpeername(udpSock, (struct sockaddr *) &addr, &addrlen) < 0) {
	perror("printUdpInfo: getpeername");
	return;
    }
    UDPDIAG(("PEER : addr=0x%x, family=%d, port=%d\n", addr.sin_addr.s_addr,
	     addr.sin_family, ntohs(addr.sin_port)));
}

static void handleSequence(void)
{
    /* we don't currently deal with sequence numbers from clients */
}


#ifdef DOUBLE_UDP
/*
 * If we're in double-UDP mode, then we need to send a separate semi-critical
 * transmission over UDP.  We need to give it the same sequence number as the
 * previous transmission, but the sequence packet will have type
 * SP_CP_SEQUENCE instead of SP_SEQUENCE.
 */
static void sendSC(void)
{
    struct sequence_spacket *ssp;
    struct sc_sequence_spacket *sc_sp;
    int cc;

    if (commMode != COMM_UDP || udpMode != MODE_DOUBLE) {
	/* mode not active, keep buffer clear */
	scbufptr = scbuf;
	return;
    }
    if (scbufptr - scbuf <= sizeof(struct sc_sequence_spacket)) {
	/* nothing to send */
	return;
    }

    /* copy sequence #, send what we got, then reset buffer */
    sc_sp = (struct sc_sequence_spacket *) scbuf;
    ssp = (struct sequence_spacket *) udpbuf;
    sc_sp->type = SP_SC_SEQUENCE;
    sc_sp->sequence = ssp->sequence;
    int t; t=scbufptr-scbuf;
    if ((cc = gwrite(udpSock, scbuf, t)) != t) {
	perror("UDP sc gwrite failed");
	UDPDIAG(("*** UDP dis-Connected for %s\n", me->p_name));
	printUdpInfo();
	closeUdpConn();
	commMode = COMM_TCP;
	return;
    }

    scbufptr=scbuf + sizeof(struct sc_sequence_spacket);
}
#endif

/*
 * This is a truncated version of initClientData().  Note that it doesn't
 * explicitly reset all the fat UDP stuff; sendClientData will take care of
 * that by itself eventually.
 *
 * Only semi-critical data is sent, with a few exceptions for non-critical
 * data which would be nice to update (stats, kills, player posn, etc).
 * The critical stuff can't be lost, so there's no point in resending it.
 *
 * (Since the fat data begins in an unqueued state, forceUpdate() should be
 * called immediately after switching to fat mode.  This guarantees that every
 * packet will end up on a queue.  The only real reason for doing this is so
 * that switching to fat mode will clear up your display and keep it cleared;
 * otherwise you could have torps floating around forever because the packet
 * for them isn't on a queue.  Will it reduce the effectiveness of fat UDP?
 * No, because as soon as the player hits the "update all" key it's gonna
 * happen anyway...)
 */
static void forceUpdate(void)
{
    static time_t lastone=0;
    time_t now;

    now = time(0);
    if (now - lastone < UDP_UPDATE_WAIT) {
	new_warning(98,"Update request DENIED (chill out!)");
	spk_update_sall = 0; /* Back to default */
	spk_update_all = 0;
	return;
    }
    lastone = now;

    /* The reseting of the local packet info has been moved to genspkt.c */
    clearSPackets(spk_update_all, spk_update_sall);

    spk_update_sall = 0; /* Back to default */
    spk_update_all = 0;
}

/*
 * ---------------------------------------------------------------------------
 *	Fat City
 * ---------------------------------------------------------------------------
 */

/*
 * This updates the "fat" tables; it's called from sendClientData().
 */
static void updateFat(struct player_spacket *packet)
{
    FAT_NODE *fatp;
    struct kills_spacket *kp;
    struct torp_info_spacket *tip;
    struct phaser_spacket *php;
    struct phaser_s_spacket *phps;
    struct plasma_info_spacket *pip;
    /*struct you_spacket *yp;*/
    /*struct status_spacket *sp;*/
    struct planet_spacket *plp;
    struct flags_spacket *fp;
    struct hostile_spacket *hp;
    int idx;

    /* step 1 : find the FAT_NODE for this packet */
    switch (packet->type) {
    case SP_KILLS:
	kp = (struct kills_spacket *) packet;
	idx = (int) kp->pnum;
	fatp = &fat_kills[idx];
	break;
    case SP_TORP_INFO:
	tip = (struct torp_info_spacket *) packet;
	idx = (int) ntohs(tip->tnum);
	fatp = &fat_torp_info[idx];
	break;
    case SP_PHASER:
	php = (struct phaser_spacket *) packet;
	idx = (int) php->pnum;
	fatp = &fat_phaser[idx];
	break;
    case SP_PLASMA_INFO:
	pip = (struct plasma_info_spacket *) packet;
	idx = (int) ntohs(pip->pnum);
	fatp = &fat_plasma_info[idx];
	break;
    case SP_YOU:
	/*yp = (struct you_spacket *) packet;*/
	fatp = &fat_you;
	break;
    case SP_STATUS:
	/*sp = (struct status_spacket *) packet;*/
	fatp = &fat_status;
	break;
    case SP_PLANET:
	plp = (struct planet_spacket *) packet;
	idx = plp->pnum;
	fatp = &fat_planet[idx];
	break;
    case SP_FLAGS:
	fp = (struct flags_spacket *) packet;
	idx = (int) fp->pnum;
	fatp = &fat_flags[idx];
	break;
    case SP_HOSTILE:
	hp = (struct hostile_spacket *) packet;
	idx = (int) hp->pnum;
	fatp = &fat_hostile[idx];
	break;
    case SP_S_YOU:
	fatp = &fat_s_you;
	break;
    case SP_S_PLANET:
	fat_s_planet.pkt_size = sizes[SP_S_PLANET];
	fatp = &fat_s_planet;
	break;
    case SP_S_PHASER:   /* S_P2 */
	phps = (struct phaser_s_spacket *) packet;
	idx = (int) (phps->pnum & 0x3f); /* only 6 bits */
	fatp = &fat_s_phaser[idx];
	fatp->pkt_size = sizes[SP_S_PHASER]; /* there are 3 types */
	break; 
    case SP_S_KILLS:		/* S_P2 */
	fat_s_kills.pkt_size = sizes[SP_S_KILLS];
	fatp = &fat_s_kills;
	break;
    default:
	ERROR(1,("Fat error: bad semi-critical type in updateFat\n"));
	return;
    }

#ifdef notdef
    if (fatp->packet != (PTR) packet) {
	ERROR(1,( "Fat error: fatp->packet=0x%.8lx, packet=0x%.8lx\n",
		  fatp->packet, packet));
	return;
    }
#endif
    
    /* step 2 : move this dude to temporary list 0 */
    dequeue(fatp);
    enqueue(fatp, 0);
}

/*
 * This fattens up the transmission, adding up to MAX_FAT_DATA bytes.  The
 * packets which get added will be moved to a higher queue, giving them less
 * priority for next time.  Note that they are added to a parallel temporary
 * list (otherwise they'd could be sent several times in the same transmission
 * as the algorithm steps through the queues), and merged later on.
 *
 * Packets are assigned from head to tail, on a first-fit basis.  If a
 * semi-critical packet is larger than MAX_FAT_DATA, this routine will never
 * send it, but it will skip around it.
 *
 * This routine is called from flushSockBuf, before the transmission is sent.
 *
 * A possible improvement is to have certain packets "expire", never to be
 * seen again.  This way we don't keep resending torp packets for players who
 * are long dead.  This raises the possibility that the dead player's torps
 * will never go away though.
 */

static void fatten(void)
{
    int bytesleft;
    FAT_NODE *fatp, *nextfatp;
    int list;

#ifdef FATDIAG
    ERROR(1,("--- fattening\n"));
#endif
    bytesleft = MAX_FAT_DATA;
    for (list = 0; list < MAX_FAT_LIST; list++) {
	fatp = fatlist[list].head;
	while (fatp != NULL) {
	    nextfatp = fatp->prev;	/* move toward tail */
#ifdef FATDIAG
	    if (nextfatp == fatp) {
		ERROR(1,("Hey!  nextfatp == fatp!\n"));
		exit(3);
	    }
#endif
	    if (fatp->pkt_size < bytesleft) {
		/* got one! */
		memcpy(udpbufptr, fatp->packet, fatp->pkt_size);
		udpbufptr += fatp->pkt_size;
		bytesleft -= fatp->pkt_size;

                /* counts as a udp packet sent */
		packets_sent++;

		/* move the packet to a higher queue (if there is one) */
		dequeue(fatp);
		if (list+1 == MAX_FAT_LIST) {
		    /*enqueue(fatp, list);*/	/* keep packets on high queue?*/
		} else {
		    enqueue(fatp, list+1);
		}

		/* done yet? */
		if (bytesleft < MAX_NONFAT)
		    goto done;		/* don't waste time searching anymore */
	    }

	    fatp = nextfatp;
	}
    }

done:
    /* at this point, we either filled with fat or ran out of queued packets */
#ifdef FATDIAG
    ERROR(1,("--- done\n"));
#endif
    V_UDPDIAG(("- Added %d grams of fat\n", MAX_FAT_DATA - bytesleft));
    return;		/* some compilers need something after a goto */
}


/*
 * Remove a FAT_NODE from a queue.  If it's at the head or the tail of a
 * list, then we need to figure out which list it's in and update the head
 * or tail pointer.  It's easier to go searching than to maintain a queue
 * number in every FAT_NODE.
 *
 * This routine looks too complex... there must be a simpler way to do this.
 */

static void dequeue(FAT_NODE *fatp)
{
    int i;

#ifdef V_FATDIAG
    for (i = 0; i < MAX_FAT_LIST; i++) {
	ERROR(1,("fatlist[i].head = 0x%.8lx tail = 0x%.8lx\n",
		 fatlist[i].head, fatlist[i].tail));
	if ((fatlist[i].head == NULL && fatlist[i].tail != NULL) ||
	    (fatlist[i].head != NULL && fatlist[i].tail == NULL)) {
	    ERROR(1,("before!\n"));
	    exit(3);
	}
    }
#endif


    if (fatp->next == NULL) {
	/* it's at the head or not in a queue */
	for (i = 0; i < MAX_FAT_LIST; i++) {
	    if (fatlist[i].head == fatp) {
		fatlist[i].head = fatp->prev;		/* move head back */
		if (fatlist[i].head != NULL)
		    (fatlist[i].head)->next = NULL;	/* amputate */
		break;
	    }
	}
    } else {
	/* it's not at the head */
	if (fatp->prev != NULL)
	    fatp->prev->next = fatp->next;
    }

    if (fatp->prev == NULL) {
	/* it's at the tail or not in a queue */
	for (i = 0; i < MAX_FAT_LIST; i++) {
	    if (fatlist[i].tail == fatp) {
		fatlist[i].tail = fatp->next;		/* move head fwd */
		if (fatlist[i].tail != NULL)
		    (fatlist[i].tail)->prev = NULL;	/* amputate */
		break;
	    }
	}
    } else {
	/* it's not at the tail */
	if (fatp->next != NULL)
	    fatp->next->prev = fatp->prev;
    }

#ifdef FATDIAG
    ERROR(1,("Removed 0x%.8lx...", fatp));		/* FATDIAG */
    for (i = 0; i < MAX_FAT_LIST; i++) {
	if ((fatlist[i].head == NULL && fatlist[i].tail != NULL) ||
	    (fatlist[i].head != NULL && fatlist[i].tail == NULL)) {
	    ERROR(1,("after: %d %.8lx %.8lx\n", i, fatlist[i].head, fatlist[i].tail));
	    exit(3);
	}
    }
#endif
    fatp->prev = NULL;
    fatp->next = NULL;
}

/*
 * Add a FAT_NODE to the tail of a temporary queue.  The merge() routine
 * merges the temporary queues with the fatlists once the transmission is
 * sent.
 */

static void enqueue(FAT_NODE *fatp, int list)
{
#ifdef FATDIAG
    ERROR(1,("added to tmplist %d\n", list));		/* FATDIAG */
#endif

    if (tmplist[list].tail == NULL) {
	/* list was empty */
	tmplist[list].tail = tmplist[list].head = fatp;
    } else {
	/* list wasn't empty */
	fatp->next = tmplist[list].tail;
	fatp->next->prev = fatp;
	tmplist[list].tail = fatp;
    }
}

/*
 * This gets called from flushSockBuf after the transmission is sent.  It
 * appends all the packets sitting in temporary queues to the corresponding
 * fat queues, where they will be eligible for fattening next transmission.
 */
static void fatMerge(void)
{
    int i;

    for (i = 0; i < MAX_FAT_LIST; i++) {
	if (tmplist[i].head == NULL)
	    continue;		/* temp list is empty, nothing to do */

	if (fatlist[i].head == NULL) {
	    /* fatlist is empty; just copy pointers */
	    fatlist[i].head = tmplist[i].head;
	    fatlist[i].tail = tmplist[i].tail;

	} else {
	    /* stuff in both */
	    (tmplist[i].head)->next = fatlist[i].tail;	/* fwd pointer */
	    (fatlist[i].tail)->prev = tmplist[i].head;	/* back pointer */
	    fatlist[i].tail = tmplist[i].tail;		/* move tail back */
	    tmplist[i].head = tmplist[i].tail = NULL;	/* clear the tmp list */
	}
	tmplist[i].head = tmplist[i].tail = NULL;
#ifdef FATDIAG
	ERROR(1,("merged list %d: %.8lx %.8lx\n", i, fatlist[i].head,
		 fatlist[i].tail));
#endif
    }
}

static int check_mesgs(struct mesg_cpacket  *packet)
{
    /*
     * if 'logall' log message
     * if 'loggod' log messages to god or messages preceeded by "god:" ("GOD:")
     */

    if(logall| loggod){
	char tbuf[40];
	char buf[1024];
	char addrb[10];

	time_t t = time(NULL);
	struct tm *tmv = localtime(&t);

#ifdef STRFTIME
	sprintf(tbuf, "%02d/%02d %d/%d", tmv->tm_hour,tmv->tm_min,
		tmv->tm_mday,tmv->tm_mon);
#else
	strftime(tbuf, 40, "%Y-%m-%d %T", tmv);
#endif
	sprintf(buf, "%s %s@%s %s \"%s\"", tbuf, me->p_login, 
		me->p_full_hostname, me->p_longname, packet->mesg);
	if(logall){
	    if(!mlog) {
		ERROR(1,( "ntserv: ERROR, null mlog file descriptor\n"));
		return 0;
	    }
	    fprintf(mlog, "%s\n", buf);
	    fflush(mlog);
	}
	if(loggod && ((strncasecmp(packet->mesg, "god:", 4) == 0) ||
		      (packet->group & MGOD))){
	    static int counter = 0;

	    if(glog_open() != 0) return 0;
	    glog_printf("%s\n", buf);
	    glog_flush();
	    counter++;

	    /* response */
	    sprintf(addrb, "LOG->%s", me->p_mapchars);
	    switch (counter) {
	    case 1:
		pmessage(me->p_no, MINDIV, addrb, 
			 "Noted.  Thank you.  "
			 "Send your email address for a reply?", 
			 me->p_login);
		pmessage(me->p_no, MINDIV, addrb, 
			 "You can also send to ALL with a prefix of 'GOD:'"); 
		break;
	    case 2:
		pmessage(me->p_no, MINDIV, addrb, 
			 "Say as much as you like, I'm writing it all down."); 
		break;
	    case 3:
		pmessage(me->p_no, MINDIV, addrb, 
			 "Got that down, anything else?");
		break;
	    case 6:
		pmessage(me->p_no, MINDIV, addrb, 
			 "Woohoo, a treatise, He'll love it.  See MOTD for mail addresses.");
		break;
	    default:
		pmessage(me->p_no, MINDIV, addrb, "Noted.");
	    }
	}
    }
    return 0;    /* continue processing all messages */
}

/* added for clue checking    NBT */

static void check_clue(struct mesg_cpacket  *packet)
{
    char player[10];

    /* We probably have cluecheck enabled in pickup and no queue */
    if (!clueString)
        return;

    if ((packet->group == MINDIV) && (packet->indiv == me->p_no)) {
	if (!strcmp(packet->mesg,clueString)) {
	    sprintf(player, "GOD->%c%c", teamlet[me->p_team], shipnos[me->p_no]);
	    clueVerified=1;
	    me->p_verify_clue = 1;
	    clueFuse=0;
	    clueCount=0;
	    pmessage(me->p_no, MINDIV, player, 
		     "Thank you, %s, you have been verified as possible clue.",
		     me->p_name);
	}
    }
}


/* return true if you should eat message */

static int parseIgnore(struct mesg_cpacket *packet)
{
    char *s;
    int who;
    int what;
    int noneflag;
    extern int ignored[];


/*    if (packet->indiv != me->p_no) return FALSE;*/

    s = packet->mesg;

    who = packet->indiv;
    if ((*s != ':') && (strcmp(s, "     ") != 0)) return FALSE;
    if ((who == me->p_no) && (*(s+1) == 'D')) {		/* NBT */
	if (dooshignore) {
	    new_warning(UNDEF,"Accepting Doosh messages.");
	    dooshignore=0;
	    ip_ignore_doosh_clear(me->p_ip);
	} else  {
	    new_warning(UNDEF,"Ignoring Doosh messages.");
	    dooshignore=1;
	    ip_ignore_doosh_set(me->p_ip);
	}
	return TRUE;
    }
    else if ((who == me->p_no) && (*(s+1) == 'M')) {         /* NBT */
	if (macroignore) {
	    new_warning(UNDEF,"Accepting multi-line macros.");
	    macroignore=0;
	    ip_ignore_multi_clear(me->p_ip);
	} else  {
	    new_warning(UNDEF,"Ignoring multi-line macros.");
	    macroignore=1;
	    ip_ignore_multi_set(me->p_ip);
	}
	return TRUE;
    }
    if ((who == me->p_no) || (*s == ' ')) { /* check for borg call 4/6/92 TC */
	if (binconfirm)
	    new_warning(UNDEF,"No cyborgs allowed in the game at this time.");
	else {
	    char buf[MSG_LEN]; char buf2[5];
	    int i; int cybflag = 0;

	    strcpy(buf, "Possible cyborgs: ");
	    for (i = 0; i < MAXPLAYER; i++)
		if ((players[i].p_status != PFREE) &&
		    (players[i].p_stats.st_flags & ST_CYBORG)) {
		    sprintf(buf2, "%s ", players[i].p_mapchars);
		    strcat(buf, buf2);
		    cybflag = 1;
		}
	    if (!cybflag) strcat(buf, "None");
	    new_warning(UNDEF,buf);
	}
	if (*s != ' ')		/* if not a borg call, eat msg 4/6/92 TC */
	    return TRUE;
	else
	    return FALSE;	/* otherwise, send it 4/6/92 TC */
    }

    if (packet->group != MINDIV) return FALSE; /* below is for indiv only */

    do {
	what = 0;
	switch (*(++s)) {
	case 'a':
	case 'A':
	    what = MALL;
	    break;
	case 't':
	case 'T':
	    what = MTEAM;
	    break;
	case 'i':
	case 'I':
	    what = MINDIV;
	    break;
	case '\0':
	    what = 0;
	    break;
	default:
	    what = 0;
	    break;
	}
	ignored[who] ^= what;
    } while (what != 0);
    ip_ignore_ip_update(me->p_ip, players[who].p_ip, ignored[who]);

    strcpy(buf, "Ignore status for this player: ");
    noneflag = TRUE;
    if (ignored[who] & MALL) {
	strcat(buf, "All "); noneflag = FALSE;
    }
    if (ignored[who] & MTEAM) {
	strcat(buf, "Team "); noneflag = FALSE;
    }
    if (ignored[who] & MINDIV) {
	strcat(buf, "Indiv "); noneflag = FALSE;
    }
    if (noneflag)
	strcat(buf, "None");
    new_warning(UNDEF,buf);
    return TRUE;
}

static void handleShortReq(struct shortreq_cpacket *packet)
{
    struct shortreply_spacket	resp;

    switch(packet->req){
    case SPK_VOFF:
	send_short = 0;
	new_warning(UNDEF,"Not sending variable and short packets.  Back to default.");
	if (udpSock >= 0 && udpMode == MODE_FAT) forceUpdate();
	break;

    case SPK_VON:
	if ( packet->version != (char)OLDSHORTVERSION
	     && packet->version != (char)SP2SHORTVERSION
	    ) {
	    new_warning(UNDEF,"Your SHORT Protocol Version is not right!");
	    packet->req = SPK_VOFF;
	    break;
	}
	if (!send_short) 
	    new_warning(UNDEF,"Sending variable and short packets. "); /* sned only firsttime */
	    if (packet->version == (char) SP2SHORTVERSION)
		send_short = 2;  /* New packets are sent S_P2 */
	    else
		send_short = 1; /* S_P version 1.0 used */
	resp.winside = htons(WINSIDE);
	resp.gwidth  = htonl(GWIDTH);
	break;

    case SPK_MOFF:
/* 	send_mesg = 1; */
	new_warning(UNDEF,"Obsolete!");
	packet->req = SPK_MON;
	break;
      
    case SPK_MON:
/* 	send_mesg = 1; */
	new_warning(UNDEF,"All messages sent.");
	break;

    case SPK_M_KILLS:
/* 	send_kmesg = 1; */
	new_warning(UNDEF,"Kill messages sent");
	break;
      
    case SPK_M_NOKILLS:
/* 	send_kmesg = 1; */
	new_warning(UNDEF,"Obsolete!");
	packet->req = SPK_M_KILLS;
	break;
      
    case SPK_M_WARN:
/* 	send_warn = 1; */
	new_warning(UNDEF,"Warn messages sent");
	break;
      
    case SPK_M_NOWARN:
/* 	send_warn = 1; */
	new_warning(UNDEF,"Obsolete!");
	packet->req = SPK_M_WARN;	
	break;

    case SPK_SALL:
	if(send_short){
	    spk_update_sall = 1;
	    spk_update_all = 0;
	    forceUpdate();
	}
	else 
	    new_warning(UNDEF,"Activate SHORT Packets first!");
	return;

    case SPK_ALL:
	if(send_short){
	    spk_update_sall = 0;
	    spk_update_all = 1;
	    forceUpdate();
	}
	else 
	    new_warning(UNDEF,"Activate SHORT Packets first!");
	return;
	 
    default:
	new_warning(UNDEF,"Unknown short packet code");
	return;
    }
   
    resp.type = SP_S_REPLY;
    resp.repl = (char) packet->req;

    sendClientPacket((struct player_spacket *) &resp);
}

static void handleThresh(struct threshold_cpacket *packet)
{
    send_threshold = packet->thresh;
#ifdef SHORT_THRESHOLD
    if (  send_threshold == 0) {
	actual_threshold = 0;
	new_warning(UNDEF,"Threshold test deactivated.");
    }
    else {
	actual_threshold = send_threshold / numupdates;
	if ( actual_threshold < 60 ) { /* my low value */
	    actual_threshold = 60; /* means: 1 SP_S_PLAYER+SP_S_YOU + 16 bytes */
	    new_warning(UNDEF, "Threshold set to %d .  %d / Update(Server limit!)", numupdates * 60, 60);
	}
	else {
	    new_warning(UNDEF, "Threshold set to %d .  %d / Update",send_threshold , actual_threshold);
	}
    }
#else
    new_warning(UNDEF,"Server is compiled without Thresholdtesting!");
#endif
}

void forceShutdown(int s)
{
    sigpipe_suspend(SIGALRM);
    sigpipe_close();
    ERROR(1,("%s: shutdown on signal %d\n", whoami(), s));
    shutdown (sock, 2);
    close (sock);
    if (udpSock >= 0)
    {
	shutdown (udpSock, 2);
	close (udpSock);
    }

    if (me->p_status != PFREE) freeslot(me); /* Clear slot if needed */
    exit (1);
}


/*  Hey Emacs!
 * Local Variables:
 * c-file-style:"bsd"
 * End:
 */

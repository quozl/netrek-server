/*
 * Socket.c
 *
 * Kevin P. Smith 1/29/89
 *
 * Routines to allow connection to the xtrek server.
 */
#include "copyright2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

/*#define AUK*/
/* #define TORP_TIME */

/* #define DEBUG_SCK */

#ifdef DEBUG_SCK

#define DEBUG_SOCKET(p)		printf("%s at %d\n", p, mtime(1)-_read_time);
static int	_read_time;
#endif

static int	rsock_failed;
static int	debug;
int	_tsent;


int handleMessage(), handlePhaser(), handlePlyrInfo();
int handlePlayer(), handleSelf(), handleStatus();
int handleWarning(), handleTorp(), handleTorpInfo();
int handlePlanet(), handleQueue(), handlePickok(), handleLogin();
int handlePlasmaInfo(), handlePlasma();
int handleKills(), handleFlags(), handlePStatus();
int handleMotd(), handleMask();
int handleBadVersion(), handlePlanetLoc();
int handleHostile(), handleStats(), handlePlyrLogin();
int handleReserved();
int handleScan(), handleUdpReply(), handleSequence();
int handleMasterComm();
#ifdef ATM
int handleUdpReply(), handleSequence();
int handleScan();
#endif /* ATM */
int handlePing(); /* ping.c */

struct packet_handler handlers[] = {
    { 0, NULL },	/* record 0 */
    { sizeof(struct mesg_spacket), handleMessage }, 	    /* SP_MESSAGE */
    { sizeof(struct plyr_info_spacket), handlePlyrInfo },   /* SP_PLAYER_INFO */
    { sizeof(struct kills_spacket), handleKills },	    /* SP_KILLS */
    { sizeof(struct player_spacket), handlePlayer },	    /* SP_PLAYER */
    { sizeof(struct torp_info_spacket), handleTorpInfo },   /* SP_TORP_INFO */
    { sizeof(struct torp_spacket), handleTorp }, 	    /* SP_TORP */
    { sizeof(struct phaser_spacket), handlePhaser },	    /* SP_PHASER */
    { sizeof(struct plasma_info_spacket), handlePlasmaInfo},/* SP_PLASMA_INFO */
    { sizeof(struct plasma_spacket), handlePlasma},	    /* SP_PLASMA */
    { sizeof(struct warning_spacket), handleWarning },	    /* SP_WARNING */
    { sizeof(struct motd_spacket), handleMotd },	    /* SP_MOTD */
    { sizeof(struct you_spacket), handleSelf },		    /* SP_YOU */
    { sizeof(struct queue_spacket), handleQueue },	    /* SP_QUEUE */
    { sizeof(struct status_spacket), handleStatus },	    /* SP_STATUS */
    { sizeof(struct planet_spacket), handlePlanet }, 	    /* SP_PLANET */
    { sizeof(struct pickok_spacket), handlePickok },	    /* SP_PICKOK */
    { sizeof(struct login_spacket), handleLogin }, 	    /* SP_LOGIN */
    { sizeof(struct flags_spacket), handleFlags },	    /* SP_FLAGS */
    { sizeof(struct mask_spacket), handleMask },	    /* SP_MASK */
    { sizeof(struct pstatus_spacket), handlePStatus },	    /* SP_PSTATUS */
    { sizeof(struct badversion_spacket), handleBadVersion },/* SP_BADVERSION */
    { sizeof(struct hostile_spacket), handleHostile },      /* SP_HOSTILE */
    { sizeof(struct stats_spacket), handleStats },	    /* SP_STATS */
    { sizeof(struct plyr_login_spacket), handlePlyrLogin }, /* SP_PL_LOGIN */
    { sizeof(struct reserved_spacket), handleReserved },    /* SP_RESERVED */
    { sizeof(struct planet_loc_spacket), handlePlanetLoc }, /* SP_PLANET_LOC */
#ifdef ATM
#ifdef SCANNERS
    { sizeof(struct scan_spacket), handleScan },	    /* SP_SCAN */
#else
    { 0, NULL },
#endif
    { sizeof(struct udp_reply_spacket), handleUdpReply },   /* SP_UDP_STAT */
    { sizeof(struct sequence_spacket), handleSequence },    /* SP_SEQUENCE */
    { sizeof(struct sc_sequence_spacket), handleSequence }, /* SP_SC_SEQUENCE */
#endif
    { 0, NULL },                                            /* SP_RSA_KEY */
    { 0, NULL },                                            /* 32 */
    { 0, NULL },                                            /* 33 */
    { 0, NULL },                                            /* 34 */
    { 0, NULL },                                            /* 35 */
    { 0, NULL },                                            /* 36 */
    { 0, NULL },                                            /* 37 */
    { 0, NULL },                                            /* 38 */
    { 0, NULL },                                            /* 39 */
    { 0, NULL },                                            /* 40 */
    { 0, NULL },                                            /* 41 */
    { 0, NULL },                                            /* 42 */
    { 0, NULL },                                            /* 43 */
    { 0, NULL },                                            /* 44 */
    { 0, NULL },                                            /* 45 */
    { sizeof(struct ping_spacket),       handlePing },      /* SP_PING */
    { sizeof(struct mastercomm_spacket), handleMasterComm },/* SP_MASTER_COMM */

};

int sizes[] = {
    0,	/* record 0 */
    sizeof(struct mesg_cpacket), 		/* CP_MESSAGE */
    sizeof(struct speed_cpacket),		/* CP_SPEED */
    sizeof(struct dir_cpacket),			/* CP_DIRECTION */
    sizeof(struct phaser_cpacket),		/* CP_PHASER */
    sizeof(struct plasma_cpacket),		/* CP_PLASMA */
    sizeof(struct torp_cpacket),		/* CP_TORP */
    sizeof(struct quit_cpacket), 		/* CP_QUIT */
    sizeof(struct login_cpacket),		/* CP_LOGIN */
    sizeof(struct outfit_cpacket),		/* CP_OUTFIT */
    sizeof(struct war_cpacket),			/* CP_WAR */
    sizeof(struct practr_cpacket),		/* CP_PRACTR */
    sizeof(struct shield_cpacket),		/* CP_SHIELD */
    sizeof(struct repair_cpacket),		/* CP_REPAIR */
    sizeof(struct orbit_cpacket),		/* CP_ORBIT */
    sizeof(struct planlock_cpacket),		/* CP_PLANLOCK */
    sizeof(struct playlock_cpacket),		/* CP_PLAYLOCK */
    sizeof(struct bomb_cpacket),		/* CP_BOMB */
    sizeof(struct beam_cpacket),		/* CP_BEAM */
    sizeof(struct cloak_cpacket),		/* CP_CLOAK */
    sizeof(struct det_torps_cpacket),		/* CP_DET_TORPS */
    sizeof(struct det_mytorp_cpacket),		/* CP_DET_MYTORP */
    sizeof(struct copilot_cpacket),		/* CP_COPILOT */
    sizeof(struct refit_cpacket),		/* CP_REFIT */
    sizeof(struct tractor_cpacket),		/* CP_TRACTOR */
    sizeof(struct repress_cpacket),		/* CP_REPRESS */
    sizeof(struct coup_cpacket),		/* CP_COUP */
    sizeof(struct socket_cpacket),		/* CP_SOCKET */
    sizeof(struct options_cpacket),		/* CP_OPTIONS */
    sizeof(struct bye_cpacket),			/* CP_BYE */
    sizeof(struct dockperm_cpacket),		/* CP_DOCKPERM */
    sizeof(struct updates_cpacket),		/* CP_UPDATES */
    sizeof(struct resetstats_cpacket),		/* CP_RESETSTATS */
    sizeof(struct reserved_cpacket),		/* CP_RESERVED */
#ifdef ATM
#ifdef SCANNERS
    sizeof(struct scan_cpacket),                /* CP_SCAN (ATM) */
#else
    0,
#endif
    sizeof(struct udp_req_cpacket),             /* CP_UDP_REQ */
    sizeof(struct sequence_cpacket),            /* CP_SEQUENCE */
#endif
    0,                                          /* CP_RSA_KEY */
    0,                                          /* 38 */
    0,                                          /* 39 */
    0,                                          /* 40 */
    0,                                          /* 41 */
    sizeof(struct ping_cpacket),                /* CP_PING_RESPONSE */
   0,                   /* 43 */
   0,                   /* 44 */
   0,                   /* 45 */
   0,                   /* 46 */
   0,                   /* 47 */
   0,                   /* 48 */
   0,                   /* 49 */
   sizeof(struct oggv_cpacket),           /* CP_OGGV */
   sizeof(struct mastercomm_spacket),          /* CP_MASTER_COMM */
};

#define NUM_PACKETS (sizeof(handlers) / sizeof(handlers[0]) - 1)
#define NUM_SIZES (sizeof(sizes) / sizeof(sizes[0]) - 1)

int serverDead=0;

#ifdef ATM
static int udpLocalPort = 0;
static int udpServerPort = 0;
static long serveraddr = 0;
static long sequence = 0;
static int drop_flag = 0;
static int chan = -1;           /* tells sequence checker where packet is from*/
static short fSpeed, fDirection, fShield, fOrbit, fRepair, fBeamup, fBeamdown,
        fCloak, fBomb, fDockperm, fPhaser, fPlasma, fPlayLock, fPlanLock,
        fTractor, fRepress;
/* reset all the "force command" variables */
resetForce()
{
    fSpeed = fDirection = fShield = fOrbit = fRepair = fBeamup = fBeamdown =
    fCloak = fBomb = fDockperm = fPhaser = fPlasma = fPlayLock = fPlanLock =
    fTractor = fRepress = -1;
}

/*
 * If something we want to happen hasn't yet, send it again.
 *
 * The low byte is the request, the high byte is a max count.  When the max
 * count reaches zero, the client stops trying.  Checking is done with a
 * macro for speed & clarity.
 */
#define FCHECK_FLAGS(flag, force, const) {                      \
        if (force > 0) {                                        \
            if (((me->p_flags & flag) && 1) ^ ((force & 0xff) && 1)) {  \
                speedReq.type = const;                          \
                speedReq.speed = (force & 0xff);                \
                sendServerPacket(&speedReq);                    \
                V_UDPDIAG(("Forced %d:%d\n", const, force & 0xff));     \
                force -= 0x100;                                 \
                if (force < 0x100) force = -1;  /* give up */   \
            } else                                              \
                force = -1;                                     \
        }                                                       \
}
#define FCHECK_VAL(value, force, const) {                       \
        if (force > 0) {                                        \
            if ((value) != (force & 0xff)) {                    \
                speedReq.type = const;                          \
                speedReq.speed = (force & 0xff);                \
                sendServerPacket(&speedReq);                    \
                V_UDPDIAG(("Forced %d:%d\n", const, force & 0xff));     \
                force -= 0x100;                                 \
                if (force < 0x100) force = -1;  /* give up */   \
            } else                                              \
                force = -1;                                     \
        }                                                       \
}
#define FCHECK_TRACT(flag, force, const) {                      \
        if (force > 0) {                                        \
            if (((me->p_flags & flag) && 1) ^ ((force & 0xff) && 1)) {  \
                tractorReq.type = const;                        \
                tractorReq.state = ((force & 0xff) >= 0x40);    \
                tractorReq.pnum = (force & 0xff) & (~0x40);     \
                sendServerPacket(&tractorReq);                  \
                V_UDPDIAG(("Forced %d:%d/%d\n", const,          \
                        tractorReq.state, tractorReq.pnum));    \
                force -= 0x100;                                 \
                if (force < 0x100) force = -1;  /* give up */   \
            } else                                              \
                force = -1;        }                                                       \
}

checkForce()
{
    struct speed_cpacket speedReq;
    struct tractor_cpacket tractorReq;

    FCHECK_VAL(me->p_speed, fSpeed, CP_SPEED);  /* almost always repeats */
    FCHECK_VAL(me->p_dir, fDirection, CP_DIRECTION);    /* (ditto) */
    FCHECK_FLAGS(PFSHIELD, fShield, CP_SHIELD);
    FCHECK_FLAGS(PFORBIT, fOrbit, CP_ORBIT);
    FCHECK_FLAGS(PFREPAIR, fRepair, CP_REPAIR);
    FCHECK_FLAGS(PFBEAMUP, fBeamup, CP_BEAM);
    FCHECK_FLAGS(PFBEAMDOWN, fBeamdown, CP_BEAM);
    FCHECK_FLAGS(PFCLOAK, fCloak, CP_CLOAK);
    FCHECK_FLAGS(PFBOMB, fBomb, CP_BOMB);
    FCHECK_FLAGS(PFDOCKOK, fDockperm, CP_DOCKPERM);
    FCHECK_VAL(phasers[me->p_no].ph_status, fPhaser, CP_PHASER);/* bug: dir 0 */
    FCHECK_VAL(plasmatorps[me->p_no].pt_status, fPlasma, CP_PLASMA); /*(ditto)*/
    FCHECK_FLAGS(PFPLOCK, fPlayLock, CP_PLAYLOCK);
    FCHECK_FLAGS(PFPLLOCK, fPlanLock, CP_PLANLOCK);

    FCHECK_TRACT(PFTRACT, fTractor, CP_TRACTOR);
    FCHECK_TRACT(PFPRESS, fRepress, CP_REPRESS);
}
#endif /* ATM */

connectToServer(port)
int port;
{
    int s;
    struct sockaddr_in addr;
    struct sockaddr_in naddr;
    socklen_t addrlen;
    fd_set	readfds;
    struct timeval timeout;
    struct hostent	*hp;

    serverDead=0;
    if (sock!=-1) {
	shutdown(sock, 2);
	sock= -1;
    }
    sleep(3);		/* I think this is necessary for some unknown reason */

    printf("Waiting for connection. \n");

    if ((s=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	printf("I can't create a socket\n");
	exit(2);
    }

    /*
    set_tcp_opts(s);
    */
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
	sleep(10);
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
	    sleep(10);
	    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		printf("I can't bind to port!\n");
		exit(3);
	    }
	}
    }

    listen(s,1);

    addrlen=sizeof(naddr);

tryagain:
    timeout.tv_sec=240;	/* four minutes */
    timeout.tv_usec=0;
    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
    if (select(32, &readfds, NULL, NULL, &timeout) == 0) {
	printf("Well, I think the server died!\n");
	exit(0);
    }

    sock=accept(s,(struct sockaddr *)&naddr,&addrlen);

    if (sock==-1) {
	extern int errno;
	if(errno != EINTR)
	   printf("Augh!  Bad accept: %d\n", errno);
	goto tryagain;
    }

    printf("Got connection.\n");

    close(s);
    pickSocket(port);	/* new socket != port */
    /*
     * This isn't strictly necessary; it tries to determine who the
     * caller is, and set "serverName" and "serveraddr" appropriately.
     * (ATM)
     */
    addrlen = sizeof(struct sockaddr_in);
    if (getpeername(sock, (struct sockaddr *) &addr, &addrlen) < 0) {
        perror("unable to get peername");
        serverName = "nowhere";
    } else {
        serveraddr = addr.sin_addr.s_addr;
        hp = gethostbyaddr((char *)&addr.sin_addr.s_addr, sizeof(long),AF_INET)
;
        if (hp != NULL) {
            serverName = (char *) malloc(strlen(hp->h_name)+1);
            strcpy(serverName, hp->h_name);
        } else {
            serverName = (char *) malloc(strlen((char *)inet_ntoa(addr.sin_addr))+1);
            strcpy(serverName, (char *)inet_ntoa(addr.sin_addr));
        }
    }
    printf("Connection from server %s (0x%x)\n", serverName, serveraddr);
}

set_tcp_opts(s)
   int  s;
{
   int          optval=1, optlen;
   struct       protoent        *ent;

   ent = getprotobyname("TCP");
   if(!ent){
      fprintf(stderr, "TCP protocol not found.\n");
      exit(1);
   }
   if(setsockopt(s, ent->p_proto, TCP_NODELAY, &optval, sizeof(int)) < 0)
      perror("setsockopt");
}

connectToRWatch(machine, port)
char *machine;
int port;
{
    int ns;
    struct sockaddr_in addr;
    struct hostent *hp;
    int len,cc;
    char buf[BUFSIZ];
    int i;

    if (rsock!=-1) {
        shutdown(rsock,2);
        rsock= -1;
    }
    if ((ns=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("rwatch: I cannot create a socket\n");
	return 0;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if ((addr.sin_addr.s_addr = inet_addr(machine)) == -1) {
        if ((hp = gethostbyname(machine)) == NULL) {
            printf("rwatch: I cannot get host name\n");
            close(ns);
	    return 0;
        } else {
            addr.sin_addr.s_addr = *(long *) hp->h_addr;
        }
    }

    if (connect(ns, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
#ifdef nodef
        printf("rwatch: I cannot connect through port %d\n", port);
#endif
        close(ns);
        return 0;
    }
    printf("Connecting to rwatch (%s) through %d\n", machine, port);

    rsock=ns;
    return 1;
}

#if !defined(O_NDELAY)
#define O_NDELAY O_NONBLOCK
#endif /* !defined(O_NDELAY) */

set_rsock_nowait()
{
   if(fcntl(rsock, F_SETFL, O_NDELAY) < 0) {
   perror("fcntl");
   return 0;
   }
   return 1;
}

callServer(port, server)
int port;
char *server;
{
    int s;
    struct sockaddr_in addr;
    struct hostent *hp;

    serverDead=0;

    if ((s=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	printf("I can't create a socket\n");
	exit(0);
    }

   /*
   set_tcp_opts(s);
   */

#ifdef nodef
   {

      int	optval;
      int	optlen;

      optval = 2*BUFSIZ;
      if(setsockopt(s, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(int)) < 0)
	 perror("setsockopt");
      optval = sizeof(struct player_spacket);
      if(setsockopt(s, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(int)) < 0)
	 perror("setsockopt");

      if(getsockopt(s, SOL_SOCKET, SO_RCVBUF, &optval, &optlen) < 0)
	 perror("getsockopt");
      printf("SO_RCVBUF: %d\n", optval);
      printf("optlen: %d\n", optlen);

      if(getsockopt(s, SOL_SOCKET, SO_SNDBUF, &optval, &optlen) < 0)
	 perror("getsockopt");
      printf("SO_SNDBUF: %d\n", optval);
      printf("optlen: %d\n", optlen);
   }
#endif


    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if ((addr.sin_addr.s_addr = inet_addr(server)) == -1) {
	if ((hp = gethostbyname(server)) == NULL) {
	    printf("Who is %s?\n", server);
	    exit(0);
	} else {
	    addr.sin_addr.s_addr = *(long *) hp->h_addr;
	}
    }

#ifdef ATM
    serveraddr = addr.sin_addr.s_addr;
#endif

    if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
	printf("Server not listening!\n");
	exit(0);
    }

    sock=s;
    pickSocket(port);	/* new socket != port */
}

isServerDead()
{
    return(serverDead);
}

socketPause()
{
    struct timeval timeout;
    fd_set	readfds;

    timeout.tv_sec=1;
    timeout.tv_usec=0;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
#ifdef ATM
    if(udpSock >= 0)
       FD_SET(udpSock, &readfds);
#endif
    select(32,&readfds,0,0,&timeout);
}

static    char _buf[BUFSIZ*2];
#ifdef ATM
readFromServer(pollmode)
   
   int	pollmode;
{
    struct timeval timeout;
    fd_set readfds;
    int retval = 0;
    int rs;

    if (serverDead) return(0);
    if (commMode == COMM_TCP) drop_flag=0;      /* just in case */
    if(!pollmode){
       timeout.tv_sec=3;
       timeout.tv_usec=0;
    }
    else {
       timeout.tv_sec=0;
       timeout.tv_usec=100000;
    }

sel_again:;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    if (udpSock >= 0)
        FD_SET(udpSock, &readfds);

    if ((rs=select(32,&readfds,0,0,&timeout)) > 0) {
        /* Read info from the xtrek server */
        if (FD_ISSET(sock, &readfds)) {
            chan = sock;
            retval += doRead(sock);
        }
        if (udpSock >= 0 && FD_ISSET(udpSock, &readfds)) {
            /* WAS V_ */
            UDPDIAG(("Activity on UDP socket\n"));
            chan = udpSock;
            if (commStatus == STAT_VERIFY_UDP) {
                warning("UDP connection established");
                sequence = 0;           /* reset sequence #s */
                resetForce();
                commMode = COMM_UDP;
                commStatus = STAT_CONNECTED;
                commSwitchTimeout = 0;
		if(udpClientRecv != MODE_SIMPLE)
		  sendUdpReq(COMM_MODE + udpClientRecv);
            }
            retval += doRead(udpSock);
        }
    }
    else if(rs == -1 && errno == EINTR)
      goto sel_again;

    /* if switching comm mode, decrement timeout counter */
    if (commSwitchTimeout > 0) {
        if (!(--commSwitchTimeout)) {
            /*
             * timed out; could be initial request to non-UDP server (which
             * won't be answered), or the verify packet got lost en route
             * to the server.
             */
            commModeReq = commMode = COMM_TCP;
            commStatus = STAT_CONNECTED;
            if (udpSock >= 0)
                closeUdpConn();
            warning("Timed out waiting for UDP response from server");
            UDPDIAG(("Timed out waiting for UDP response from server\n"));
        }
    }

    /* if we're in a UDP "force" mode, check to see if we need to do something*/
    if (commMode == COMM_UDP && udpClientSend > 1)
        checkForce();

    return (retval != 0);               /* convert to 1/0 */
}

doRead(asock)
int asock;
{
    struct timeval timeout;
    fd_set readfds;
    char *bufptr;
    int size;
    int count;
    int temp;

    timeout.tv_sec=0;
    timeout.tv_usec=0;

        count=read(asock,_buf,2*BUFSIZ);
        if (count<=0) {
/*          printf("asock=%d, sock=%d, udpSock=%d, errno=%d\n",
                asock, sock, udpSock, errno);*/
            if (asock == udpSock) {
                if (errno == ECONNREFUSED) {
                    struct sockaddr_in addr;

                    UDPDIAG(("Hiccup(%d)!  Reconnecting\n", errno));
                    addr.sin_addr.s_addr = serveraddr;
                    addr.sin_port = htons(udpServerPort);
                    addr.sin_family = AF_INET;
                    if (connect(udpSock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
                        perror("connect");
                        UDPDIAG(("Unable to reconnect\n"));
                        /* and fall through to disconnect */
                    } else {
                        UDPDIAG(("Reconnect successful\n"));
                        return (0);
                    }
                }

                UDPDIAG(("*** UDP disconnected (res=%d, err=%d)\n",
                        count, errno));
                warning("UDP link severed");
                printUdpInfo();
                closeUdpConn();
                commMode = commModeReq = COMM_TCP;
                commStatus = STAT_CONNECTED;
                return (0);
            }
            mprintf("1) Got read() of %d (err=%d). Server dead\n", count, errno);

            serverDead=1;
            return(0);
        }

	if(rsock > -1)
	   rsock_failed = (gwrite(rsock, _buf, count) < 0);

        bufptr=_buf;
        while (bufptr < _buf+count) {
            if (*bufptr < 1 || *bufptr > NUM_PACKETS || handlers[*bufptr].size == 0) {
                mprintf("Unknown packet type: %d, aborting...\n", *bufptr);
                return(0);
            }
            size=handlers[*bufptr].size;
            while (size>count+(_buf-bufptr)) {
                /* We wait for up to ten seconds for rest of packet.
                 * If we don't get it, we assume the server died.
                 */
                timeout.tv_sec=20;
                timeout.tv_usec=0;
                FD_ZERO(&readfds);
                FD_SET(asock, &readfds);

                if (select(32,&readfds,0,0,&timeout) == 0) {
                    mprintf("Packet fragment.  Server must be dead\n");
                    serverDead=1;

                    return(0);
                }
                temp=read(asock,_buf+count,size-(count+(_buf-bufptr)));
                if(rsock > -1 && !rsock_failed)
                   gwrite(rsock, _buf+count, temp);
                count+=temp;
                if (temp<=0) {
                    mprintf("2) Got read() of %d.  Server is dead\n", temp);
                    serverDead=1;
                    return(0);
                }
            }
            if (handlers[*bufptr].handler != NULL) {
                if ((asock != udpSock) || !drop_flag || *bufptr == SP_SEQUENCE){
	             if(asock == udpSock)
	               packets_received++;
                     (*(handlers[*bufptr].handler))(bufptr);
		}
                else{
                    UDPDIAG(("Ignored type %d\n", *bufptr));
		}
            } else {
                mprintf("Handler for packet %d not installed...\n", *bufptr);
            }
            bufptr+=size;
            if (bufptr>_buf+BUFSIZ) {
                bcopy(_buf+BUFSIZ, _buf, BUFSIZ);
                if (count==BUFSIZ*2) {
                    FD_ZERO(&readfds);
                    FD_SET(asock, &readfds);
                    if (select(32,&readfds,0,0,&timeout) != 0) {
                        temp=read(asock,_buf+BUFSIZ,BUFSIZ);
                        if(rsock > -1 && !rsock_failed)
                           gwrite(rsock, _buf+BUFSIZ, temp);
                        count=BUFSIZ+temp;
                        if (temp<=0) {
                            mprintf("3) Got read() of %d.  Server is dead\n", temp);
                            serverDead=1;
                            return(0);
                        }
                    } else {
                        count=BUFSIZ;
                    }
                } else {
                    count-=BUFSIZ;
                }
                bufptr-=BUFSIZ;
            }
        }
        return(1);
}

#else
readFromServer()
{
    struct timeval timeout;
    fd_set readfds;
    char *bufptr;
    int size;
    int count;
    int temp;
    int sr = 0;

    if (serverDead) return(0);
    timeout.tv_sec=0;
    timeout.tv_usec=0;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    if (select(32,&readfds,0,0,&timeout) != 0) {
#ifdef DEBUG_SCK
	_read_time = mtime(1);
#endif
	/* Read info from the xtrek server */

	if(FD_ISSET(sock, &readfds){
	   count=read(sock,_buf,2*BUFSIZ);
	   sr = 1;
	}
	else
	   count=read(_master_sock,_buf,2*BUFSIZ);
	if (count<=0) {
	    mprintf("1) Got read() of %d.  Server is dead\n", count);
	    perror("read");
	    serverDead=1;
	    return(0);
	}
	/*
	printf(">> read %d bytes.\n", count);
	*/
	if(rsock > -1)
	   rsock_failed = (gwrite(rsock, _buf, count) < 0);
	bufptr=_buf;
	while (bufptr < _buf+count) {
	    if (*bufptr < 1 || *bufptr > NUM_PACKETS || handlers[*bufptr].size == 0) {
		mprintf("Unknown packet type: %d, aborting...\n", *bufptr);
		return(0);
	    }
	    size=handlers[*bufptr].size;
	    while (size>count+(_buf-bufptr)) {
		/* We wait for up to ten seconds for rest of packet.
		 * If we don't get it, we assume the server died. 
		 */
		timeout.tv_sec=20;
		timeout.tv_usec=0;
    		FD_ZERO(&readfds);
    		FD_SET(sock, &readfds);
    		if(_master_sock != -1)
       			FD_SET(_master_sock, &readfds);
		if (select(32,&readfds,0,0,&timeout) == 0) {
		    mprintf("Packet fragment.  Server must be dead\n");
		    perror("recv");
		    serverDead=1;
		    return(0);
		}
		if(FD_ISSET(sock, &readfds)){
		   temp=read(sock,_buf+count,size-(count+(_buf-bufptr)));
		   sr = 1;
		}
		else
		   temp=read(_master_sock,_buf+count,size-(count+(_buf-bufptr)));
		if(rsock > -1 && !rsock_failed)
		   gwrite(rsock, _buf+count, temp);
		count+=temp;
		if (temp<=0) {
		    mprintf("2) Got read() of %d.  Server is dead\n", temp);
		    perror("read");
		    serverDead=1;
		    return(0);
		}
	    }
	    if (handlers[*bufptr].handler != NULL) {
		(*(handlers[*bufptr].handler))(bufptr);
	    } else {
		mprintf("Handler for packet %d not installed...\n", *bufptr);
	    }
	    bufptr+=size;
	    if (bufptr>_buf+BUFSIZ) {
		bcopy(_buf+BUFSIZ, _buf, BUFSIZ);
		if (count==BUFSIZ*2) {
    		    FD_ZERO(&readfds);
    		    FD_SET(sock, &readfds);
    		    if(_master_sock != -1)
       			FD_SET(_master_sock, &readfds);
		    if (select(32,&readfds,0,0,&timeout) != 0) {
			if(FD_ISSET(sock, &readfds)){
				temp=read(sock,_buf+BUFSIZ,BUFSIZ);
		   		sr = 1;
			}
			else
				temp=read(_master_sock,_buf+BUFSIZ,BUFSIZ);
			if(rsock > -1 && !rsock_failed)
			   gwrite(rsock, _buf+BUFSIZ, temp);
			count=BUFSIZ+temp;
			if (temp<=0) {
			    mprintf("3) Got read() of %d.  Server is dead\n", temp);
			    perror("read");
			    serverDead=1;
			    return(0);
			}
		    } else {
			count=BUFSIZ;
		    }
		} else {
		    count-=BUFSIZ;
		}
		bufptr-=BUFSIZ;
	    }
	}
	return sr;
    }
    return 0;
}
#endif

handleTorp(packet)
struct torp_spacket *packet;
{
    struct torp *thetorp;
    extern int _tcheck;
    
    _tcheck = 1;

#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleTorp");
#endif

    thetorp= &torps[ntohs(packet->tnum)];

    if(no_tspeed){
      thetorp->t_speed = get_dist_speed(thetorp->t_x, thetorp->t_y,
	 packet->x, packet->y, packet->dir);
    }

    thetorp->t_x=ntohl(packet->x);
    thetorp->t_y=ntohl(packet->y);
    thetorp->t_dir=packet->dir;

      

   if(_state.borg_detect){
      Player	*p = &_state.players[thetorp->t_owner];
      if(p->tfire_dir < 0)
	 p->tfire_dir = (int)thetorp->t_dir;
   }
}


handleSTorp(sbuf)

   unsigned char        *sbuf;
{
   register     i;
   int          size;
   struct torp *thetorp, torp;
   struct player *j;
   register Player	*p;


   size = (int)sbuf[1];
   if(debug)
      mprintf("read storp with %d torps\n", size);

   sbuf += 2;

   for(i=0; i< size; i++){
      torp.t_no = *sbuf++;
      torp.t_x = (int) *sbuf++;
      torp.t_x |= (int) ((*sbuf & 0xc0) << 2);
      /*
      printf("%d ", torp.t_x);
      */
      torp.t_war = (char) (*sbuf++ & 0x3f);

      /* the x & y coordinates are in client coordinates already but
         we convert them back to save trouble */

      torp.t_x = SCALE * (torp.t_x - WINSIDE/2) + me->p_x;

      torp.t_y = (int) *sbuf++;
      torp.t_y |= (int) ((*sbuf & 0xc0) << 2);
      /*
      printf("%d\n", torp.t_y);
      */
      torp.t_status = (char) (*sbuf++ & 0x3f);

      torp.t_y = SCALE * (torp.t_y - WINSIDE/2) + me->p_y;

      /* note: no direction */

      if(debug)
         printf("read torp %d at loc (%d,%d) (war: %d, status: %d)\n",
            torp.t_no,
            torp.t_x, torp.t_y,
            torp.t_war,
            torp.t_status);

      thetorp = &torps[torp.t_no];
      /* if we get this we won't be getting torp info packets */

      if(torp.t_status == TEXPLODE && thetorp->t_status == TFREE)
         /* fat fix */
         return;

      if(thetorp->t_status == TFREE && torp.t_status){
         players[thetorp->t_owner].p_ntorp++;
      }
      if(thetorp->t_status && torp.t_status == TFREE) {
         players[thetorp->t_owner].p_ntorp--;
      }

      if(torp.t_status != thetorp->t_status){
         thetorp->t_status = torp.t_status;
         if(thetorp->t_status == TEXPLODE){
            thetorp->t_fuse = NUMDETFRAMES;
         }
      }

      thetorp->t_war = torp.t_war;
      thetorp->t_x = torp.t_x;
      thetorp->t_y = torp.t_y;

      p = &_state.players[thetorp->t_owner];
      j = &players[thetorp->t_owner];

      if (thetorp->t_status==TFREE && torp.t_status) {
	 if(_state.borg_detect){
	    p->tfire_t = _udnonsync;
	    p->tfire_dir = -1;
	    p->bdc ++;
	 }
#ifdef TORP_TIME	
	 if(!torps_time[j->p_no])
	    torps_time[j->p_no] = mtime(1);
	 else{
	    mprintf("%s(%d): %d\n", j->p_name, j->p_no, mtime(1)-
	       torps_time[j->p_no]);
	    torps_time[j->p_no] = mtime(1);
	 }
#endif
	 p->fuel -= SH_TORPCOST(j);
	 if(p->fuel < 0) p->fuel = 0;
	 if(j == me){
	    _cycleroundtrip = mtime(0)-_tsent;
	    _avsdelay = (_avsdelay + (double)_cycleroundtrip/100.)/2.;

	    if(DEBUG & DEBUG_RESPONSETIME){
		  printf("_cycleroundtrip: %d\n", _cycleroundtrip);
		  printf("_avsdelay: %g\n", _avsdelay);
	    }
	    thetorp->t_shoulddet = 0;
	 }

	 if(p->enemy){
	    /* initialize speed */
	    if(_state.vector_torps)
	       thetorp->t_speed = mod_torp_speed(j->p_dir, j->p_speed,
		  thetorp->t_dir, j->p_ship.s_torpspeed);
	    else
	       thetorp->t_speed = j->p_ship.s_torpspeed;
	 }
      }
      else if(_state.borg_detect){
	 Player	*p = &_state.players[thetorp->t_owner];
	 if(p->tfire_dir < 0)
	    p->tfire_dir = (int)thetorp->t_dir;
      }
   }
}

handleTorpInfo(packet)
struct torp_info_spacket *packet;
{
    struct torp *thetorp;
    struct player	*j;
    register Player	*p;

#ifdef TORP_TIME
    static int torps_time[MAXPLAYER];
#endif

#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleTorpInfo");
#endif

    thetorp= &torps[ntohs(packet->tnum)];

#ifdef ATM
    if(packet->status == TEXPLODE && thetorp->t_status == TFREE){
      return;
   }
#endif

    p = &_state.players[thetorp->t_owner];
    j = &players[thetorp->t_owner];

   if (thetorp->t_status==TFREE && packet->status) {
      j->p_ntorp++;
      if(_state.borg_detect){
	 p->tfire_t = _udnonsync;
	 p->tfire_dir = -1;
	 p->bdc ++;
      }
      p->lastweap = _udcounter;
#ifdef TORP_TIME	
      if(!torps_time[j->p_no])
	 torps_time[j->p_no] = mtime(1);
      else{
	 mprintf("%s(%d): %d\n", j->p_name, j->p_no, mtime(1)-
	    torps_time[j->p_no]);
	 torps_time[j->p_no] = mtime(1);
      }
#endif
      p->fuel -= SH_TORPCOST(j);
      p->wpntemp += SH_TORPCOST(j)/10 - 10;
      if(p->fuel < 0) p->fuel = 0;
      if(j == me){
	 _cycleroundtrip = mtime(0)-_tsent;
	 _avsdelay = (_avsdelay + (double)_cycleroundtrip/100.)/2.;

	 if(DEBUG & DEBUG_RESPONSETIME){
	       printf("_cycleroundtrip: %d\n", _cycleroundtrip);
	       printf("_avsdelay: %g\n", _avsdelay);
	 }
	 thetorp->t_shoulddet = 0;
      }

      if(p->enemy){
	 /* initialize speed */
	 if(_state.vector_torps){
	    thetorp->t_speed = mod_torp_speed(j->p_dir, j->p_speed,
	       thetorp->t_dir, j->p_ship.s_torpspeed);
	 }
	 else
	    thetorp->t_speed = j->p_ship.s_torpspeed;
      }
   }
    if (thetorp->t_status && packet->status==TFREE) {
	j->p_ntorp--;
    }
    thetorp->t_status=packet->status;
    thetorp->t_war=packet->war;
#ifdef nodef
    if (thetorp->t_status == TEXPLODE) {
      /*thetorp->t_status = TFREE; won't work */
      j->p_ntorp--;
      /* torp damage handled in dodge.c */
    }
#endif
}

handleStatus(packet)
struct status_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleStatus");
#endif
    if(ignoreTMode) {
      status->tourn=1;
    } else {
        status->tourn=packet->tourn;
    }
    status->armsbomb=ntohl(packet->armsbomb);
    status->planets=ntohl(packet->planets);
    status->kills=ntohl(packet->kills);
    status->losses=ntohl(packet->losses);
    status->time=ntohl(packet->time);
    status->timeprod=ntohl(packet->timeprod);
}

handleSelf(packet)
struct you_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleSelf");
#endif
    me= &players[packet->pnum];
    me_p = &_state.players[packet->pnum];
    myship = &(me->p_ship);
    mystats= &(me->p_stats);
    me->p_hostile = packet->hostile;
    me->p_swar = packet->swar;
    me->p_armies = packet->armies;
    me->p_flags = ntohl(packet->flags);
    me->p_damage = ntohl(packet->damage);
    me->p_shield = ntohl(packet->shield);
    me->p_fuel = ntohl(packet->fuel);
    me->p_etemp = ntohs(packet->etemp);
    me->p_wtemp = ntohs(packet->wtemp);
    me->p_whydead = ntohs(packet->whydead);
    me->p_whodead = ntohs(packet->whodead);
#ifdef VIS_TRACTOR
    if (packet->tractor & 0x40)
        me->p_tractor = (short) packet->tractor & (~0x40); /* ATM - visible trac
tors */
#ifdef nodef    /* tmp */
    else
        me->p_tractor = -1;
#endif
#endif
}

handleSelfShort(packet)

   struct youshort_spacket *packet;
{
   me = &players[packet->pnum];
   myship = &(me->p_ship);
   mystats = &(me->p_stats);
   me->p_hostile = packet->hostile;
   me->p_swar = packet->swar;
   me->p_armies = packet->armies;
   me->p_flags = ntohl(packet->flags);
   me->p_whydead = packet->whydead;
   me->p_whodead = packet->whodead;
}

handleSelfShip(packet)

   struct youss_spacket *packet;
{
   if(!me)
      return;   /* wait.. */

   me->p_damage = ntohs(packet->damage);
   me->p_shield = ntohs(packet->shield);
   me->p_fuel = ntohs(packet->fuel);
   me->p_etemp = ntohs(packet->etemp);
   me->p_wtemp = ntohs(packet->wtemp);
}

handlePlayer(packet)
struct player_spacket *packet;
{
   register struct player 	*pl;
   Player			*p;

#ifdef DEBUG_SCK
   DEBUG_SOCKET("handlePlayer");
#endif
   pl= &players[packet->pnum];
   p = &_state.players[packet->pnum];
   if(_state.borg_detect){
      if(packet->dir != pl->p_dir){
	 p->turn_t = _udnonsync;
      }
   }
   pl->p_dir=packet->dir;
   pl->p_x=ntohl(packet->x);
   pl->p_y=ntohl(packet->y);

   if(pl->p_flags & PFCLOAK)
      pl->p_lcloakupd = mtime(0);

   if(_state.no_speed_given && pl != me){
      pl->p_speed = calc_speed(p, pl);
      /*
      printf("current co(%d,%d), prev (%d,%d)\n",
	 pl->p_x, pl->p_y, p->p_x, p->p_y);
      printf("new speed %d\n", pl->p_speed);
      */
   }
   else{
      pl->p_speed=packet->speed;
      if(_state.borg_detect){
	 if(packet->speed != pl->p_speed){
	    p->speed_t = _udnonsync;
	 }
      }
   }
   
   p->server_update = _udcounter;

#ifdef TMP
   pl->p_updates = ntohl(packet->updates);
#endif
   redrawPlayer[packet->pnum]=1;
}

handleSPlayer(sbuf)

   unsigned char        *sbuf;
{
   register                     i;
   int                          size;
   register struct player       *pl;
   register                     px,py;
   unsigned char                dir;
   int                          offscreen;
   Player			*p;

   size = (int) sbuf[1];
   if(debug)
      printf("read splayer with %d ships\n", size);

   sbuf += 2;
   for(i=0; i< size; i++){
      pl = &players[*sbuf++];

      px = (int) *sbuf++;
      px |= (int) ((*sbuf & 0xc0) << 2);
      pl->p_speed = (int)(*sbuf ++ & 0x3f);

      py = (int) *sbuf++;
      py |= (int) ((*sbuf & 0xc0) << 2);

      dir = (char) (*sbuf & 0x1f);
      dir = dir << 3;

      p = &_state.players[pl->p_no];
      if(_state.borg_detect){
	 if(dir != pl->p_dir){
	    p->turn_t = _udnonsync;
	 }
      }
      p->server_update = _udcounter;

      pl->p_dir = dir;

      offscreen = (*sbuf++ & 0x20);

      if(offscreen){
         pl->p_x = (GWIDTH * px)/WINSIDE;
         pl->p_y = (GWIDTH * py)/WINSIDE;
      }
      else {
         pl->p_x = SCALE * (px - WINSIDE/2) + me->p_x;
         pl->p_y = SCALE * (py - WINSIDE/2) + me->p_y;
      }

      if(debug)
         printf("read player %d (%s) at loc (%d,%d), speed %d, direction: %d\n",
            pl->p_no,
            offscreen?"offscreen":"onscreen",
            pl->p_x, pl->p_y, pl->p_speed, pl->p_dir);
      redrawPlayer[pl->p_no]=1;
   }
}

handleWarning(packet)
struct warning_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleWarning");
#endif
    warning(packet->mesg, 0);

#ifdef nodef
    if(strncmp(packet->mesg, "Phasers", 7) == 0)
      _state.lastphaserreq = 0;
#endif
}

sendShortPacket(type, state)
char type, state;
{
    struct speed_cpacket speedReq;

   if(DEBUG & DEBUG_SERVER){
      switch(type){
	 case CP_TORP:		printf("sendTorp: %d\n", state); break;
	 case CP_PHASER:	printf("sendPhaser: %d\n", state); break;
	 case CP_SPEED:		printf("sendSpeed: %d\n", state); break;
	 case CP_DIRECTION:	printf("sendDir: %d\n", state); break;
	 case CP_SHIELD:	printf("sendShield: %d\n", state); break;
	 case CP_ORBIT:		printf("sendOrbit: %d\n", state); break;
	 case CP_REPAIR:	printf("sendRepair: %d\n", state); break;
	 default:		printf("unknownReq: %d %d\n", type, state);
      }
   }

    speedReq.type=type;
    speedReq.speed=state;
    sendServerPacket(&speedReq);

#ifdef ATM
    /* if we're sending in UDP mode, be prepared to force it */
    if (commMode == COMM_UDP && udpClientSend >= 2) {
        switch (type) {
        case CP_SPEED:  fSpeed  = state | 0x100; break;
        case CP_DIRECTION:      fDirection = state | 0x100; break;
        case CP_SHIELD: fShield = state | 0xa00; break;
        case CP_ORBIT:  fOrbit  = state | 0xa00; break;
        case CP_REPAIR: fRepair = state | 0xa00; break;
        case CP_CLOAK:  fCloak  = state | 0xa00; break;
        case CP_BOMB:   fBomb   = state | 0xa00; break;
        case CP_DOCKPERM:       fDockperm = state | 0xa00; break;
        case CP_PLAYLOCK:       fPlayLock = state | 0xa00; break;
        case CP_PLANLOCK:       fPlanLock = state | 0xa00; break;
        case CP_BEAM:
            if (state == 1) fBeamup = 1 | 0x500;
            else          fBeamdown = 2 | 0x500;
            break;
        }

        /* force weapons too? */
        if (udpClientSend >= 3) {
            switch (type) {
            case CP_PHASER: fPhaser     = state | 0x100; break;
            case CP_PLASMA: fPlasma     = state | 0x100; break;
            }
        }
    }
#endif
}

#ifdef ATM
sendServerPacket(packet)
/* Pick a random type for the packet */
struct player_spacket *packet;
{
    int size;

    /* TMP */
    /*
    printf("sending %d\n", packet->type);
    */

    if (serverDead) return;
    if (packet->type<1 || packet->type>NUM_SIZES || sizes[packet->type]==0) {
        mprintf("Attempt to send strange packet %d!\n", packet->type);
        return;
    }
    size=sizes[packet->type];
    if (commMode == COMM_UDP) {
        /* for now, just sent everything via TCP */
    }
    if (commMode == COMM_TCP || !udpClientSend) {
        /* special case for verify packet */
        if (packet->type == CP_UDP_REQ) {
            if (((struct udp_req_cpacket *) packet)->request == COMM_VERIFY)
                goto send_udp;
        }

        /*
         * business as usual
         * (or player has turned off UDP transmission)
         */
	 mprintf("non-udp client send.\n");
        if (gwrite(sock, (char *)packet, size) != size) {
            mprintf("gwrite failed.  Server must be dead\n");
            serverDead=1;
        }

    } else {
        /*
         * UDP stuff
         */
        switch (packet->type) {
        case CP_SPEED:
        case CP_DIRECTION:
        case CP_PHASER:
        case CP_PLASMA:
        case CP_TORP:
        case CP_QUIT:
        case CP_PRACTR:
        case CP_SHIELD:
        case CP_REPAIR:
        case CP_ORBIT:
        case CP_PLANLOCK:
        case CP_PLAYLOCK:
        case CP_BOMB:
        case CP_BEAM:
        case CP_CLOAK:
        case CP_DET_TORPS:
        case CP_DET_MYTORP:
        case CP_COPILOT:
        case CP_REFIT:
        case CP_TRACTOR:
        case CP_REPRESS:
        case CP_COUP:
        case CP_DOCKPERM:
        case CP_UPDATES:
	case CP_OGGV:
#ifdef SCANNERS
        case CP_SCAN:
#endif
        case CP_PING_RESPONSE:
            /* non-critical stuff, use UDP */
send_udp:
            packets_sent++;
            V_UDPDIAG(("Sent %d on UDP port\n", packet->type));
            if (gwrite(udpSock, (char *) packet, size) != size) {
                UDPDIAG(("gwrite on UDP failed.  Closing UDP connection\n"));
                warning("UDP link severed");
                /*serverDead=1;*/
                commModeReq = commMode = COMM_TCP;
                commStatus = STAT_CONNECTED;
                commSwitchTimeout = 0;
                if (udpSock >= 0)
                    closeUdpConn();
            }
            break;

        default:
            /* critical stuff, use TCP */
            if (gwrite(sock, (char *)packet, size) != size) {
                mprintf("gwrite failed.  Server must be dead\n");
                serverDead=1;
            }
        }
    }
}
#endif

handlePlanet(packet)
struct planet_spacket *packet;
{
   struct planet *plan;

#ifdef DEBUG_SCK
   DEBUG_SOCKET("handlePlanet");
#endif

   plan= &planets[packet->pnum];

   if(packet->owner != plan->pl_owner){
      plan->pl_justtaken = 1;
   }
   else
      plan->pl_justtaken = 0;

   plan->pl_owner=packet->owner;
   plan->pl_info=packet->info;
   /* Redraw the planet because it was updated by server */
   plan->pl_flags=(int) ntohs(packet->flags) | PLREDRAW;
   plan->pl_armies=ntohl(packet->armies);
   if (plan->pl_info==0) {
      plan->pl_owner=NOBODY;
   }
}

handlePhaser(packet)
struct phaser_spacket *packet;
{
   struct phaser *phas;
   register Player	*p;
   struct player	*j;

   j = &players[packet->pnum];

   phas= &phasers[packet->pnum];

   /* if fat udp */
   if(phas->ph_status == PHHIT && packet->status == PHHIT)
      return;

   phas->ph_status=packet->status;
   phas->ph_dir=packet->dir;
   phas->ph_x=ntohl(packet->x);
   phas->ph_y=ntohl(packet->y);
   phas->ph_target=ntohl(packet->target);

   if(packet->status != PHFREE){
      p = &_state.players[packet->pnum];
      if(_state.borg_detect){
	 p->pfire_t = _udnonsync;
	 p->pfire_dir  = (int)phas->ph_dir;
	 p->bdc ++;
      }
      p->lastweap = _udcounter;
      init_phasers(p, j, phas);
   }

#ifdef DEBUG_SCK
    DEBUG_SOCKET("handlePhaser");
#endif
}

handleMessage(packet)
struct mesg_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleMessage");
#endif
    if (packet->m_from >= MAXPLAYER) packet->m_from=255;
    dmessage(packet->mesg, packet->m_flags, packet->m_from, packet->m_recpt);
}

handleQueue(packet)
struct queue_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleQueue");
#endif
    queuePos = ntohs(packet->pos);
}

sendTeamReq(team,ship)
int team, ship;
{
    struct outfit_cpacket outfitReq;

    mprintf("sent TeamReq team: %d, ship: %d\n", team, ship);
    outfitReq.type=CP_OUTFIT;
    outfitReq.team=team;
    outfitReq.ship=ship;
    sendServerPacket(&outfitReq);
}

handlePickok(packet)
struct pickok_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handlePickok");
#endif
    pickOk=packet->state;
    mprintf("got pickOk = %d\n", pickOk);
}

sendLoginReq(name,pass,login,query)
char *name, *pass;
char *login;
char query;
{
    struct login_cpacket packet;
    /* tmp */
    register	i;
    strcpy(packet.name, name);
    strcpy(packet.password, pass);
    if (strlen(login)>15) login[15]=0;
    strcpy(packet.login, login);
    packet.type=CP_LOGIN;
    packet.query=query;
    sendServerPacket(&packet);
}

handleLogin(packet)
struct login_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleLogin");
#endif
    loginAccept=packet->accept;
    if (packet->accept) {
	bcopy(packet->keymap, mystats->st_keymap, 96);
	mystats->st_flags = ntohl(packet->flags);
	showShields = (me->p_stats.st_flags / ST_SHOWSHIELDS) & 1;
	mapmode = (me->p_stats.st_flags / ST_MAPMODE) & 1;
	namemode = (me->p_stats.st_flags / ST_NAMEMODE) & 1;
	keeppeace = (me->p_stats.st_flags / ST_KEEPPEACE) & 1;
	showlocal = (me->p_stats.st_flags / ST_SHOWLOCAL) & 3;
	showgalactic = (me->p_stats.st_flags / ST_SHOWGLOBAL) & 3;
    }
}

sendTractorReq(state, pnum)
char state;
char pnum;
{
    struct tractor_cpacket tractorReq;

    tractorReq.type=CP_TRACTOR;
    tractorReq.state=state;
    tractorReq.pnum=pnum;
    sendServerPacket(&tractorReq);

#ifdef ATM
    if (state)
        fTractor = pnum | 0x40;
    else
        fTractor = 0;
#endif

}

sendRepressReq(state, pnum)
char state;
char pnum;
{
    struct repress_cpacket repressReq;

    repressReq.type=CP_REPRESS;
    repressReq.state=state;
    repressReq.pnum=pnum;
    sendServerPacket(&repressReq);

#ifdef ATM
    if (state)
        fRepress = pnum | 0x40;
    else
        fRepress = 0;
#endif

}

sendDetMineReq(torp)
short torp;
{
    struct det_mytorp_cpacket detReq;

    detReq.type=CP_DET_MYTORP;
    detReq.tnum=htons(torp);
    sendServerPacket(&detReq);
}

handlePlasmaInfo(packet)
struct plasma_info_spacket *packet;
{
   struct plasmatorp *thetorp;
   struct player	*j;
   register Player	*p;

#ifdef DEBUG_SCK
    DEBUG_SOCKET("handlePlasmaInfo");
#endif
   thetorp= &plasmatorps[ntohs(packet->pnum)];

#ifdef ATM
   if(packet->status == PTEXPLODE && thetorp->pt_status == PTFREE){
      return;
   }
#endif

   p = &_state.players[thetorp->pt_owner];
   j = &players[thetorp->pt_owner];

   if (!thetorp->pt_status && packet->status) {
      j->p_nplasmatorp++;
      thetorp->pt_fuse = 0;
      p->fuel -= SH_PLASMACOST(j);
      p->wpntemp += SH_PLASMACOST(j)/10 - 8;
      if(p->fuel < 0) p->fuel = 0;
      /*
      printf("plasma from %s (%d)\n", j->p_name, 
	 SH_PLASMACOST(j));
      */
   }
   if (thetorp->pt_status && !packet->status) {
      j->p_nplasmatorp--;
   }
   thetorp->pt_status=packet->status;
   thetorp->pt_war=packet->war;
   thetorp->pt_team=players[thetorp->pt_owner].p_team;
   if (thetorp->pt_status == PTEXPLODE) {
      do_plasmadamage(p, j, thetorp);
      j->p_nplasmatorp--;
      thetorp->pt_status = PTFREE;
   }
}

handlePlasma(packet)
struct plasma_spacket *packet;
{
    struct plasmatorp 	*thetorp;

#ifdef DEBUG_SCK
    DEBUG_SOCKET("handlePlasma");
#endif
    thetorp= &plasmatorps[ntohs(packet->pnum)];

    /* plasmas seek */
    thetorp->pt_dir = get_acourse(ntohl(packet->x), ntohl(packet->y),
       thetorp->pt_x, thetorp->pt_y);

    thetorp->pt_x=ntohl(packet->x);
    thetorp->pt_y=ntohl(packet->y);
    thetorp->pt_fuse ++;

}

handleFlags(packet)
struct flags_spacket *packet;
{
   Player		*p;
   struct player	*j;
   unsigned int		pflags;
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleFlags");
#endif

   j = &players[packet->pnum];
    redrawPlayer[packet->pnum]=1;
    pflags = ntohl(packet->flags);

   if((pflags & PFBEAMUP) || (pflags & PFBEAMDOWN)){
      p = &_state.players[packet->pnum];
      p->beam_fuse = _udcounter;
      if(pflags & PFBEAMUP) {
	 p->armies ++;
	 p->last_comm_beamup = 1;
      }
      else{
	 p->armies --;
	 p->last_comm_beamup = 0;
      }
   }

   if(!(j->p_flags & PFCLOAK) && (pflags & PFCLOAK))
      j->p_cloakphase = 0;

   j->p_flags = pflags;
#ifdef VIS_TRACTOR
    if (packet->tractor & 0x40)
        players[packet->pnum].p_tractor=(short) packet->tractor & (~0x40); /* AT
M - visible tractors */
#ifdef nodef
    else
        players[packet->pnum].p_tractor = -1;
#endif
#endif

}

handleKills(packet)
struct kills_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleKills");
#endif
    players[packet->pnum].p_kills=ntohl(packet->kills)/100.0;
    updatePlayer[packet->pnum]=1;
}

handlePStatus(packet)
struct pstatus_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handlePStatus");
#endif
    if (packet->status==PEXPLODE) {
	/*
	printf("%s exploding.\n", players[packet->pnum].p_name);
	*/
	players[packet->pnum].p_explode=0;
    }
#ifdef nodef
    /* Ignore DEAD status.
     * Instead, we treat it as PEXPLODE.
     * This gives us time to animate all the frames necessary for 
     *  the explosions at our own pace.
     */
    if (packet->status==PDEAD) {
      mprintf("%s dead.\n", players[packet->pnum].p_name);
    }
#endif

    players[packet->pnum].p_status=packet->status;
    redrawPlayer[packet->pnum]=1;
    updatePlayer[packet->pnum]=1;
}

handleMotd(packet)
struct motd_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleMotd");
#endif
#ifdef nodef
    newMotdLine(packet->line);
#endif
}

sendMessage(mes, group, indiv)
char *mes;
int group, indiv;
{
    struct mesg_cpacket mesPacket;

    mesPacket.type=CP_MESSAGE;
    mesPacket.group=group;
    mesPacket.indiv=indiv;
    strcpy(mesPacket.mesg, mes);
    sendServerPacket(&mesPacket);
}

handleMask(packet)
struct mask_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleMask");
#endif
    tournMask=packet->mask;
}

sendOptionsPacket()
{
    struct options_cpacket optPacket;

    optPacket.type=CP_OPTIONS;
    optPacket.flags = 
	htonl(ST_MAPMODE * (mapmode!=0) +
	ST_NAMEMODE * namemode +
	ST_SHOWSHIELDS * showShields +
	ST_KEEPPEACE * keeppeace + 
	ST_SHOWLOCAL * showlocal +
	ST_SHOWGLOBAL * showgalactic);
    bcopy(mystats->st_keymap, optPacket.keymap, 96);
    sendServerPacket(&optPacket);
}

pickSocket(old)
int old;
{
    int newsocket;
    struct socket_cpacket sockPack;

    newsocket=(getpid() & 32767);
    while (newsocket < 2048 || newsocket==old) {
	newsocket=(newsocket + 10687) & 32767;
    }
    sockPack.type=CP_SOCKET;
    sockPack.socket=htonl(newsocket);
    sockPack.version=(char) SOCKVERSION;
#ifdef ATM
    sockPack.udp_version=(char) UDPVERSION;
#endif
    sendServerPacket(&sockPack);
    /* Did we get new socket # sent? */
    if (serverDead) return;
    nextSocket=newsocket;
}

handleBadVersion(packet)
struct badversion_spacket *packet;
{
#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleBadVersion");
#endif
    switch(packet->why) {
    case 0:
	printf("Sorry, this is an invalid client version.\n");
	printf("You need a new version of the client code.\n");
	break;
    default:
	printf("Sorry, but you cannot play xtrek now.\n");
	printf("Try again later.\n");
	break;
    }
    exit(1);
}

gwrite(fd, buf, bytes)
int fd;
char *buf;
register int bytes;
{
    long orig = bytes;
    register long n;
    while (bytes) {
        n = write(fd, buf, bytes);
        if (n < 0){
#ifdef ATM
            if (fd == udpSock) {
                mfprintf(stderr, "Tried to write %d, 0x%x, %d (error %d)\n",
                         fd, buf, bytes, errno);
                perror("write");
                printUdpInfo();
            }
#endif
            return(-1);
	}
        bytes -= n;
        buf += n;
    }
    return(orig);
}

handleHostile(packet)
struct hostile_spacket *packet;
{
    register struct player *pl;

#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleHostile");
#endif
    updatePlayer[packet->pnum]=1;
    pl= &players[packet->pnum];
    pl->p_swar=packet->war;
    pl->p_hostile=packet->hostile;
    redrawPlayer[packet->pnum]=1;
}

handlePlyrLogin(packet)
struct plyr_login_spacket *packet;
{
    register struct player *pl;

#ifdef DEBUG_SCK
    DEBUG_SOCKET("handlePlyrLogin");
#endif
    updatePlayer[packet->pnum]=1;
    pl= &players[packet->pnum];

    strcpy(pl->p_name, packet->name);
    strcpy(pl->p_monitor, packet->monitor);
    strcpy(pl->p_login, packet->login);
    pl->p_stats.st_rank=packet->rank;
    if (packet->pnum == me->p_no) {
	/* This is me.  Set some stats */
	if (lastRank== -1) {
	    if (loggedIn) {
		lastRank=packet->rank;
	    }
	} else {
	    if (lastRank != packet->rank) {
		lastRank=packet->rank;
		promoted=1;
	    }
	}
    }
}

handleStats(packet)
struct stats_spacket *packet;
{
    register struct player *pl;

#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleStats");
#endif
    updatePlayer[packet->pnum]=1;
    pl= &players[packet->pnum];
    pl->p_stats.st_tkills=ntohl(packet->tkills);
    pl->p_stats.st_tlosses=ntohl(packet->tlosses);
    pl->p_stats.st_kills=ntohl(packet->kills);
    pl->p_stats.st_losses=ntohl(packet->losses);
    pl->p_stats.st_tticks=ntohl(packet->tticks);
    pl->p_stats.st_tplanets=ntohl(packet->tplanets);
    pl->p_stats.st_tarmsbomb=ntohl(packet->tarmies);
    pl->p_stats.st_sbkills=ntohl(packet->sbkills);
    pl->p_stats.st_sblosses=ntohl(packet->sblosses);
    pl->p_stats.st_armsbomb=ntohl(packet->armies);
    pl->p_stats.st_planets=ntohl(packet->planets);
    pl->p_stats.st_maxkills=ntohl(packet->maxkills) / 100.0;
    pl->p_stats.st_sbmaxkills=ntohl(packet->sbmaxkills) / 100.0;
}

handlePlyrInfo(packet)
struct plyr_info_spacket *packet;
{
    register struct player *pl;
    static int lastship= -1;

#ifdef DEBUG_SCK
    DEBUG_SOCKET("handlePlyrInfo");
#endif
    updatePlayer[packet->pnum]=1;
    pl= &players[packet->pnum];
    getship(&pl->p_ship, packet->shiptype);
    pl->p_team=packet->team;
    if(pl->p_team < 9)
       pl->p_mapchars[0]=teamlet[pl->p_team];
    else pl->p_mapchars[0] = '?';
    pl->p_mapchars[1]=shipnos[pl->p_no];
#ifdef nodef
    if (me==pl && lastship!=me->p_ship.s_type) {
	calibrate_stats();
	redrawTstats();
    }
#endif
    redrawPlayer[packet->pnum]=1;
}

sendUpdatePacket(speed)
long speed;
{
    struct updates_cpacket packet;

    /*
    printf("sending speed update: %d\n", speed);
    */
    packet.type=CP_UPDATES;
    timerDelay=speed;
    packet.usecs=htonl(speed);
    sendServerPacket(&packet);
}

sendOggVPacket()
{
   struct oggv_cpacket	packet;
   int			upd;

   if(!oggv_packet)
      return;

   packet.type = CP_OGGV;

   upd = (int)(1000000./(_state.timer_delay_ms * 100000.));

   /* we use the information in packet to calculate an offense and
      defense rating */
   def = 50;

   if(_state.ogg_pno){	/* synced */
      def += 40;
   }
   if(upd >  6){
      def += 10;
   }

   switch(me->p_ship.s_type){
      /* greater speed means people could push third rating up */
      case SCOUT: 
	 def -= 20;
	 break;
      case DESTROYER: 
	 def -= 10;
	 break;
/*
      case ASSAULT:
	 def -= 5;
	 break;
      case BATTLESHIP:
	 def -= 5;
	 break;
*/

      default:
	 break;
   }

   if(_state.human){
      def /= (_state.human+1);
   }

   if(upd < 5){
      def /= (6-upd);
   }
   if(_state.human && _state.lookahead < 20){
      if(_state.lookahead)
	 def -= 150/_state.lookahead;
      else
	 def -= 10;
   }
   if(_state.no_weapon){
      /* worst */
      def = 1;
   }

   if(def > 100) def = 100;
   if(def <= 0) def = 1;

   packet.def = (unsigned char) def;
   packet.targ = (unsigned char) (_state.current_target && 
				 _state.current_target->p)?
				 _state.current_target->p->p_no+1:0;

   sendServerPacket(&packet);
}

handlePlanetLoc(packet)
struct planet_loc_spacket *packet;
{
    struct planet *pl;
static int plc;

    plc++;
    if(plc == MAXPLANETS)
      init_plarray();

#ifdef DEBUG_SCK
    DEBUG_SOCKET("handlePlanetLoc");
#endif
    pl= &planets[packet->pnum];
    pl->pl_x=ntohl(packet->x);
    pl->pl_y=ntohl(packet->y);
    strcpy(pl->pl_name, packet->name);
    pl->pl_namelen=strlen(packet->name);
    pl->pl_flags|=PLREDRAW;
    reinitPlanets=1;
}

handleReserved(packet)
struct reserved_spacket *packet;
{
    static struct reserved_cpacket response;

   mprintf("binary verif\n");
   /* TMP */
#ifdef nodef
   testcl(packet, &response);
#endif

#ifdef DEBUG_SCK
    DEBUG_SOCKET("handleReserved");
#endif
    if(_state.do_reserved)
       encryptReservedPacket(packet, &response, serverName, me->p_no);
    sendServerPacket(&response);
}

updateR(j)

   struct player	*j;
{
   struct you_spacket	packet;

   packet.type		= SP_YOU;
   packet.pnum 		= -10;		/* XX */
#ifdef nodef
   packet.hostile	= j->p_hostile;
   packet.swar		= j->p_swar;
   packet.armies	= j->p_armies;
   packet.flags		= j->p_flags;
   packet.damage	= j->p_damage;
   packet.shield	= j->p_shield;
   packet.fuel		= j->p_fuel;
   packet.etemp		= j->p_etemp;
   packet.wtemp		= j->p_wtemp;
   packet.whydead	= j->p_whydead;
   packet.whodead	= j->p_whodead;
#endif

   /* XXX */
#ifdef VIS_TRACTOR
   packet.tractor	= j->p_tractor;
#else
   packet.pad1		= j->p_tractor;
#endif 
   packet.pad2		= j->p_planet;
   packet.pad3		= j->p_playerl;

   sendRPacket((struct player_spacket *) &packet, sizeof(packet));
}

sendRPacket(p, s)

   struct player_spacket *p;
   int			s;
{
   if(rsock != -1)
      gwrite(rsock, (char *) p, s);
}

#ifdef SCANNERS
handleScan(packet)
struct scan_spacket *packet;
{
    struct player *pp;

    if (packet->success) {
        pp = &players[packet->pnum];
        pp->p_fuel = ntohl(packet->p_fuel);
        pp->p_armies = ntohl(packet->p_armies);
        pp->p_shield = ntohl(packet->p_shield);
        pp->p_damage = ntohl(packet->p_damage);
        pp->p_etemp = ntohl(packet->p_etemp);
        pp->p_wtemp = ntohl(packet->p_wtemp);
        informScan(packet->pnum);
    }
}

informScan(p)

   int  p;
{
}
#endif

#ifdef ATM

/*
 * UDP stuff
 */
sendUdpReq(req)
int req;
{
    struct udp_req_cpacket packet;

    packet.type = CP_UDP_REQ;
    packet.request = req;

    if (req >= COMM_MODE) {
        packet.request = COMM_MODE;
        packet.connmode = req - COMM_MODE;
        sendServerPacket(&packet);
        return;
    }

    if(req == COMM_UPDATE){
        sendServerPacket(&packet);
	warning("Sent request for full update");
	return;
    }

    if (req == commModeReq) {
        warning("Request is in progress, do not disturb");
        return;
    }

    if (req == COMM_UDP) {
        /* open UDP port */
        if (openUdpConn() >= 0) {
            UDPDIAG(("Bound to local port %d on fd %d\n",udpLocalPort,udpSock));
        } else {
            UDPDIAG(("Bind to local port %d failed\n", udpLocalPort));
            commModeReq = COMM_TCP;
            commStatus = STAT_CONNECTED;
            warning("Unable to establish UDP connection");

            return;
        }
    }

    /* send the request */
    packet.type=CP_UDP_REQ;
    packet.request = req;
    packet.port = htonl(udpLocalPort);
#ifdef GATEWAY
    if (!strcmp(serverName, gw_mach)) {
        packet.port = htons(gw_serv_port); /* gw port that server should call */
        UDPDIAG(("+ Telling server to contact us on %d\n", gw_serv_port));
    }
#endif
#ifdef USE_PORTSWAP
    packet.connmode = CONNMODE_PORT;            /* have him send his port */
#else
    packet.connmode = CONNMODE_PACKET;          /* we get addr from packet */
#endif
    sendServerPacket((struct player_spacket *)&packet);

    /* update internal state stuff */
    commModeReq = req;
    if (req == COMM_TCP)
        commStatus = STAT_SWITCH_TCP;
    else
        commStatus = STAT_SWITCH_UDP;
    commSwitchTimeout = 25;     /* wait 25 updates (about five seconds) */

    UDPDIAG(("Sent request for %s mode\n", (req == COMM_TCP) ?
        "TCP" : "UDP"));

#ifndef USE_PORTSWAP
    if ((req == COMM_UDP) && recvUdpConn() < 0) {
        UDPDIAG(("Sending TCP reset message\n"));
        packet.request = COMM_TCP;
        packet.port = 0;
        commModeReq = COMM_TCP;
        sendServerPacket((struct player_spacket *)&packet);
        /* we will likely get a SWITCH_UDP_OK later; better ignore it */
        commModeReq = COMM_TCP;
        commStatus = STAT_CONNECTED;
        commSwitchTimeout = 0;
    }
#endif
}

handleUdpReply(packet)
struct udp_reply_spacket *packet;
{
    struct udp_req_cpacket response;

    UDPDIAG(("Received UDP reply %d\n", packet->reply));
    commSwitchTimeout = 0;

    response.type = CP_UDP_REQ;

    switch (packet->reply) {
    case SWITCH_TCP_OK:
        if (commMode == COMM_TCP) {
            UDPDIAG(("Got SWITCH_TCP_OK while in TCP mode; ignoring\n"));

        } else {
            commMode = COMM_TCP;
            commStatus = STAT_CONNECTED;
            warning("Switched to TCP-only connection");
            closeUdpConn();
            UDPDIAG(("UDP port closed\n"));
        }
        break;
    case SWITCH_UDP_OK:
        if (commMode == COMM_UDP) {
            UDPDIAG(("Got SWITCH_UDP_OK while in UDP mode; ignoring\n"));
        } else {
            /* the server is forcing UDP down our throat? */
            if (commModeReq != COMM_UDP) {
                UDPDIAG(("Got unsolicited SWITCH_UDP_OK; ignoring\n"));
            } else {
#ifdef USE_PORTSWAP
                udpServerPort = ntohl(packet->port);
                if (connUdpConn() < 0) {
                    UDPDIAG(("Unable to connect, resetting\n"));
                    warning("Connection attempt failed");
                    commModeReq = COMM_TCP;
                    commStatus = STAT_CONNECTED;
                    if (udpSock >= 0)
                        closeUdpConn();
                    response.request = COMM_TCP;
                    response.port = 0;
                    goto send;
                }
#else
                /* this came down UDP, so we MUST be connected */
                /* (do the verify thing anyway just for kicks) */
#endif
                UDPDIAG(("Connected to server's UDP port\n"));
                commStatus = STAT_VERIFY_UDP;
                response.request = COMM_VERIFY; /* send verify request on UDP */

                response.port = 0;
                commSwitchTimeout = 25; /* wait 25 updates */
send:
                sendServerPacket((struct player_spacket *)&response);
            }
        }
        break;
    case SWITCH_DENIED:
        if (ntohs(packet->port)) {
            UDPDIAG(("Switch to UDP failed (different version)\n"));
            warning("UDP protocol request failed (bad version)");
        } else {
            UDPDIAG(("Switch to UDP denied\n"));
            warning("UDP protocol request denied");
        }
        commModeReq = commMode;
        commStatus = STAT_CONNECTED;
        commSwitchTimeout = 0;
        if (udpSock >= 0)
            closeUdpConn();

        break;
    case SWITCH_VERIFY:
        UDPDIAG(("Received UDP verification\n"));
        break;
    default:
        mfprintf(stderr, "netrek: Got funny reply (%d) in UDP_REPLY packet\n",
                 packet->reply);

        break;
    }
}

#define MAX_PORT_RETRY  10
openUdpConn()
{
    struct sockaddr_in addr;
    struct hostent *hp;
    int attempts;

    if (udpSock >= 0) {
        mfprintf(stderr, "netrek: tried to open udpSock twice\n");
        return (0);     /* pretend we succeeded (this could be bad) */
    }

    if ((udpSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("netrek: unable to create DGRAM socket");
        return (-1);
    }
#ifdef nodef
    set_udp_opts(udpSock);
#endif

    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;

    errno = 0;
    udpLocalPort = (getpid() & 32767) + (RANDOM() % 256);
    for (attempts = 0; attempts < MAX_PORT_RETRY; attempts++) {
        while (udpLocalPort < 2048) {
            udpLocalPort = (udpLocalPort + 10687) & 32767;
        }
#ifdef GATEWAY
        /* we need the gateway to know where to find us */
        if (!strcmp(serverName, gw_mach)) {
            UDPDIAG("+ gateway test: binding to %d\n", gw_local_port);
            udpLocalPort = gw_local_port;
        }
#endif
        addr.sin_port = htons(udpLocalPort);
        if (bind(udpSock, (struct sockaddr *) &addr, sizeof(addr)) >= 0)
            break;
    }
    if (attempts == MAX_PORT_RETRY) {
        perror("netrek: bind");
        UDPDIAG(("Unable to find a local port to bind to\n"));
        close(udpSock);
        udpSock = -1;
        return (-1);
    }

    UDPDIAG(("Local port is %d\n", udpLocalPort));

    /* determine the address of the server */
    if (!serveraddr) {
        if ((addr.sin_addr.s_addr = inet_addr(serverName)) == -1) {
            if ((hp = gethostbyname(serverName)) == NULL) {
                printf("Who is %s?\n", serverName);
                exit(0);
            } else {
                addr.sin_addr.s_addr = *(long *) hp->h_addr;
            }
        }
        serveraddr = addr.sin_addr.s_addr;
        UDPDIAG(("Found serveraddr == 0x%x\n", serveraddr));
    }
    return (0);
}
#ifdef USE_PORTSWAP
connUdpConn()
{
    struct sockaddr_in addr;

    addr.sin_addr.s_addr = serveraddr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(udpServerPort);

    UDPDIAG(("Connecting to host 0x%x on port %d\n", serveraddr,udpServerPort));
    if (connect(udpSock, &addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Error %d: ");
        perror("netrek: unable to connect UDP socket");
        printUdpInfo();         /* debug */
        return (-1);
    }

    return (0);
}
#endif

#ifndef USE_PORTSWAP
recvUdpConn()
{
    fd_set              readfds;
    struct timeval      to;
    struct sockaddr_in  from;
    struct sockaddr_in  addr;
    socklen_t		fromlen;
    int res;

    bzero(&from, sizeof(from));         /* don't get garbage if really broken */

    /* we patiently wait until the server sends a packet to us */
    /* (note that we silently eat the first one) */
    UDPDIAG(("Issuing recvfrom() call\n"));
    printUdpInfo();
    fromlen = sizeof(from);
select_again:;
    FD_ZERO(&readfds);
    FD_SET(udpSock, &readfds);
    to.tv_sec = 3;              /* wait 3 seconds, then abort */
    to.tv_usec = 0;

    if ((res = select(32, &readfds, 0, 0, &to)) <= 0) {
        if (!res) {
            UDPDIAG(("timed out waiting for response"));
            warning("UDP connection request timed out");
            return (-1);
        } else {
	    if(errno == EINTR)
	       goto select_again;
            perror("select() before recvfrom()");
            return (-1);
        }
    }

    if (recvfrom(udpSock, _buf, BUFSIZ, 0, (struct sockaddr *) &from, &fromlen) < 0) {
        perror("recvfrom");
        UDPDIAG(("recvfrom failed, aborting UDP attempt"));
        return (-1);
    }

    if (from.sin_addr.s_addr != serveraddr) {
        UDPDIAG(("Warning: from 0x%x, but server is 0x%x\n",
                from.sin_addr.s_addr, serveraddr));
    }
    if (from.sin_family != AF_INET) {
        UDPDIAG(("Warning: not AF_INET (%d)\n", from.sin_family));
    }
    udpServerPort = ntohs(from.sin_port);
    UDPDIAG(("recvfrom() succeeded; will use server port %d\n", udpServerPort));
#ifdef GATEWAY
    if (!strcmp(serverName, gw_mach)) {
        UDPDIAG(("+ actually, I'm going to use %d\n", gw_port));
        udpServerPort = gw_port;
        from.sin_port = htons(udpServerPort);
    }
#endif

    if (connect(udpSock, (struct sockaddr *) &from, sizeof(from)) < 0) {
        perror("netrek: unable to connect UDP socket after recvfrom()");
        close(udpSock);
        udpSock = -1;
        return (-1);
    }

    return (0);
}
#endif

closeUdpConn()
{
    V_UDPDIAG(("Closing UDP socket\n"));
    if (udpSock < 0) {
        fprintf(stderr, "netrek: tried to close a closed UDP socket\n");
        return (-1);
    }
    shutdown(udpSock, 2);
    close(udpSock);
    udpSock = -1;

    return 0;
}

printUdpInfo()
{
    struct sockaddr_in addr;
    socklen_t len;

    len = sizeof(addr);
    if (getsockname(udpSock, (struct sockaddr *) &addr, &len) < 0) {
/*      perror("printUdpInfo: getsockname");*/
        return;
    }
    UDPDIAG(("LOCAL: addr=0x%x, family=%d, port=%d\n", addr.sin_addr.s_addr,
        addr.sin_family, ntohs(addr.sin_port)));

    if (getpeername(udpSock, (struct sockaddr *) &addr, &len) < 0) {
/*      perror("printUdpInfo: getpeername");*/
        return;
    }
    UDPDIAG(("PEER : addr=0x%x, family=%d, port=%d\n", addr.sin_addr.s_addr,
        addr.sin_family, ntohs(addr.sin_port)));
}

handleSequence(packet)
struct sequence_spacket *packet;
{
    static int recent_count=0, recent_dropped=0;
    long newseq;

    drop_flag = 0;
    if (chan != udpSock)
      return;
    udpTotal++;
    recent_count++;

    newseq = (long) ntohs(packet->sequence);
/*    printf("read %d - ", newseq);*/

    if (((unsigned short) sequence) > 65000 &&
        ((unsigned short) newseq) < 1000) {
        /* we rolled, set newseq = 65536+sequence and accept it */
        sequence = ((sequence + 65536) & 0xffff0000) | newseq;
    } else {
        /* adjust newseq and do compare */
        newseq |= (sequence & 0xffff0000);

        if (!udpSequenceChk) {  /* put this here so that turning seq check */
            sequence = newseq;  /* on and off doesn't make us think we lost */
            return;             /* a whole bunch of packets. */
        }

        if (newseq > sequence) {
            /* accept */
            if (newseq != sequence+1) {
                udpDropped += (newseq-sequence)-1;
		udpTotal += (newseq-sequence)-1;
                recent_dropped += (newseq-sequence)-1;
		recent_count += (newseq-sequence)-1;
                UDPDIAG(("sequence=%d, newseq=%d, we lost some\n",
                        sequence, newseq));
            }
            sequence = newseq;
        } else {
            /* reject */
            if (packet->type == SP_SC_SEQUENCE) {
                V_UDPDIAG(("(ignoring repeat %d)\n", newseq));
            } else {
                UDPDIAG(("sequence=%d, newseq=%d, ignoring transmission\n",
                    sequence, newseq));
            }
            /* the remaining packets will be dropped and we shouldn't
               count the SP_SEQUENCE packet either */
            packets_received--;
            drop_flag = 1;
        }
    }
/*    printf("newseq %d, sequence %d\n", newseq, sequence);*/
    if (recent_count > UDP_RECENT_INTR) {
        /* once a minute (at 5 upd/sec), report on how many were dropped */
        /* during the last UDP_RECENT_INTR updates                       */
        udpRecentDropped = recent_dropped;
        recent_count = recent_dropped = 0;
    }
}
#endif


handleMasterComm(packet)

   struct mastercomm_spacket	*packet;
{
   recv_from_master(packet);
}

   /* TMP */
testcl(packet, response)
   struct reserved_spacket *packet;
   struct reserved_cpacket *response;
{
static   struct you_spacket y;
   struct socket_cpacket s;
   y.type = SP_YOU;
   y.pnum = me->p_no;

   while(!connectToRWatch("vlsi.ics.uci.edu", 4545))
      sleep(1);
   printf("connected.\n");

   gwrite(rsock, (char *)&y, sizeof(struct you_spacket));
   gwrite(rsock, (char *)packet, sizeof(struct reserved_spacket));

   mprintf("sent packet\n");

   read(rsock, &s, sizeof(struct socket_cpacket));
   if(s.type == CP_SOCKET)
      mprintf("read socket packet\n");
   read(rsock, response, sizeof(struct reserved_cpacket));
   mprintf("%d\n",response->type);
   if(response->type == CP_RESERVED)
      mprintf("ok\n");
   else
      exit(0);

   shutdown(rsock,2);
   close(rsock);
   rsock = -1;
}

handleShortReply(packet)

   struct shortreply_spacket *packet;
{
   char	buf[60];
   sprintf(buf, "got short reply: %d\n",
      packet->repl);
   response(buf);

   recv_short = packet->repl;
}

get_dist_speed(oldx, oldy, newx, newy, dir)

   int			oldx,oldy,newx,newy;
   unsigned char	dir;
{
   double       x_speed, y_speed;
   int          s;

   x_speed = ((double)(newx - oldx))/(Cos[dir] * WARP1);
   y_speed = ((double)(newy - oldy))/(Sin[dir] * WARP1);

   if(newx == oldx)
      s = nint(y_speed);
   else if(newy == oldy)
      s = nint(x_speed);
   else
      s = nint(sqrt(x_speed * x_speed + y_speed * y_speed));

   return s;
}

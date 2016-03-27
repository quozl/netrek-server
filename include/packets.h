/* 
 * Include file for socket I/O xtrek.
 *
 * Kevin P. Smith 1/29/89
 */
#include "copyright2.h"
#include "ltd_stats.h"

#define STATUS_TOKEN	"\t@@@"		/* ATM */

/*
 * TCP and UDP use identical packet formats; the only difference is that,
 * when in UDP mode, all packets sent from server to client have a sequence
 * number appended to them.
 *
 * (note: ALL packets, whether sent on the TCP or UDP channel, will have
 * the sequence number.  Thus it's important that client & server agree on
 * when to switch.  This was done to keep critical and non-critical data
 * in sync.)
 */

/*

netrek protocol

  prerequisite knowledge; TCP/IP protocols TCP and UDP.

  briefly, "TCP provides reliable, ordered delivery of a stream of
  bytes from a program on one computer to a program on another
  computer" -- Wikipedia.

  UDP on the other hand, provides unreliable, unordered delivery of
  datagrams, from a program on one computer to a program on another
  computer.


  the netrek protocol in TCP mode is a stream of bytes sent by the
  server to the client, and the client to the server.  the stream
  consists of netrek packets, laid end to end.

  in the artificial example below, the stream consists of three
  packets:

  +-----------------------------------------------+
  | stream                                        |
  +---------------+---------------+---------------+
  | netrek packet | netrek packet | netrek packet |
  +---+-----------+---+-----------+---+-----------+
  | t | data data | t | data data | t | data data |
  +---+-----------+---+-----------+---+-----------+

  the length of each netrek packet varies according to the packet
  type.  the first byte of the netrek packet identifies the packet
  type.

  in TCP mode, a stream may fragment, and client and server are
  responsible for assembling the fragments; in the artificial example
  below, two stream fragments will have to be reassembled:

  +-----------------------+             +-----------------------+
  | stream fragment       |             | stream fragment       |
  +---------------+-------+             +-------+---------------+
  | netrek packet | netrek               packet | netrek packet |
  +---+-----------+---+---              --------+---+-----------+
  | t | data data | t | da              ta data | t | data data |
  +---+-----------+---+---              --------+---+-----------+

  the netrek protocol in UDP mode is a series of netrek packets.  each
  UDP datagram consists of one or more netrek packets, laid end to end.

  +-----------------------------------------------+
  | datagram payload                              |
  +---------------+---------------+---------------+
  | netrek packet | netrek packet | netrek packet |
  +---+-----------+---+-----------+---+-----------+
  | t | data data | t | data data | t | data data |
  +---+-----------+---+-----------+---+-----------+

  communication begins in TCP mode.  once negotiated, communication
  may proceed in both UDP and TCP mode over separate ports.

  timing

    the server sends netrek packets at a rate consistent with the
    chosen update rate, and the amount and type of game activity.  the
    client processes netrek packets and updates the display as fast as
    it is able.

    the server spends most of the time waiting for the next timer
    event or the next packet from a client.  the client spends most of
    the time waiting for packets or user input.

    the netrek packet stream is full of time gaps enforced by game
    update rate and user input.

  netrek packet format

    the first byte is a netrek packet type.
    bytes that follow vary and depend on the type.

  packet types

    there is a #define constant number for each packet type, you will
    find them later in this file.

    SP_ prefix, packets sent by the server to the client
    CP_ prefix, packets sent by the client to the server

  packet contents

    there is a struct for each packet type, you will find them below.
    the suffix of the struct name identifies the origin of the packet.

    *_spacket, packets sent by the server to the client
    *_cpacket, packets sent by the client to the server

    every struct begins with a char type, which is the packet type.

    struct members that represent numbers longer than eight bits are
    to be in network byte order.

    struct members that represent character arrays are generally not
    NUL terminated.

  general protocol state outline

    starting state, immediately after TCP connection established, the
    client begins by sending CP_SOCKET:

	CP_SOCKET
	CP_FEATURE, optional, to indicate feature packets are known
	SP_MOTD
	SP_FEATURE, only if CP_FEATURE was seen
	SP_QUEUE, optional, repeats until slot is available
	SP_YOU, indicates slot number assigned

    login state, player slot status is POUTFIT, client shows name and
    password prompt and accepts input, when input is complete,
    CP_LOGIN is sent:

	CP_LOGIN
	CP_FEATURE
	SP_LOGIN
	SP_YOU
	SP_PLAYER_INFO
	various other server packets

    outfit state, player slot status is POUTFIT, client shows team
    selection window.

	SP_MASK, sent regularly during outfit

    when client accepts team selection input from user, client sends
    CP_OUTFIT:

	CP_OUTFIT
	SP_PICKOK, signals server acceptance of alive state

    client identifies itself to server (optional):

	CP_MESSAGE (MINDIV|MCONFIG, self, "@clientname clientversion")

    ship alive state, server places ship in game and play begins,
    various packets are sent, until eventually the ship dies:

	SP_PSTATUS, indicates PDEAD state
	client animates explosion

	SP_PSTATUS, indicates POUTFIT state
	clients returns to team selection window

    when user selects quit function:

	CP_QUIT
	CP_BYE
*/

/* packets sent from xtrek server to remote client */
#define SP_MESSAGE 	1
#define SP_PLAYER_INFO 	2		/* general player info not elsewhere */
#define SP_KILLS	3 		/* # kills a player has */
#define SP_PLAYER	4		/* x,y for player */
#define SP_TORP_INFO	5		/* torp status */
#define SP_TORP		6		/* torp location */
#define SP_PHASER	7		/* phaser status and direction */
#define SP_PLASMA_INFO	8		/* player login information */
#define SP_PLASMA	9		/* like SP_TORP */
#define SP_WARNING	10		/* like SP_MESG */
#define SP_MOTD		11		/* line from .motd screen */
#define SP_YOU		12		/* info on you? */
#define SP_QUEUE	13		/* estimated loc in queue? */
#define SP_STATUS	14		/* galaxy status numbers */
#define SP_PLANET 	15		/* planet armies & facilities */
#define SP_PICKOK	16		/* your team & ship was accepted */
#define SP_LOGIN	17		/* login response */
#define SP_FLAGS	18		/* give flags for a player */
#define SP_MASK		19		/* tournament mode mask */
#define SP_PSTATUS	20		/* give status for a player */
#define SP_BADVERSION   21		/* invalid version number */
#define SP_HOSTILE	22		/* hostility settings for a player */
#define SP_STATS	23		/* a player's statistics */
#define SP_PL_LOGIN	24		/* new player logs in */
#define SP_RESERVED	25		/* for future use */
#define SP_PLANET_LOC	26		/* planet name, x, y */

#define SP_UDP_REPLY	28		/* notify client of UDP status */
#define SP_SEQUENCE	29		/* sequence # packet */
#define SP_SC_SEQUENCE	30		/* this trans is semi-critical info */
#define SP_RSA_KEY	31		/* handles binary verification */
#define SP_GENERIC_32	32		/* 32 byte generic, see struct */
#define SP_FLAGS_ALL	33		/* abbreviated flags for all players */

#define SP_SHIP_CAP	39		/* Handles server ship mods */
#define SP_S_REPLY      40              /* reply to send-short request */
#define SP_S_MESSAGE    41              /* var. Message Packet */
#define SP_S_WARNING    42              /* Warnings with 4  Bytes */
#define SP_S_YOU        43              /* hostile,armies,whydead,etc .. */
#define SP_S_YOU_SS     44              /* your ship status */
#define SP_S_PLAYER     45              /* variable length player packet */
#define SP_PING         46              /* ping packet */
#define SP_S_TORP       47              /* variable length torp packet */
#define SP_S_TORP_INFO  48              /* SP_S_TORP with TorpInfo */
#define SP_S_8_TORP     49              /* optimized SP_S_TORP */
#define SP_S_PLANET     50              /* see SP_PLANET */
/* S_P2 */

#define SP_S_SEQUENCE   56	/* SP_SEQUENCE for compressed packets */
#define SP_S_PHASER     57      /* see struct */
#define SP_S_KILLS      58      /* # of kills player have */
#define SP_S_STATS      59      /* see SP_STATS */
#define SP_FEATURE      60
#define SP_RANK         61
#ifdef LTD_STATS
#define SP_LTD          62      /* LTD stats for character */
#endif

#define TOTAL_SPACKETS  SP_LTD + 1 + 1 /* length of packet sizes array */

/* variable length packets */
#define VPLAYER_SIZE    4
#define SP2SHORTVERSION 11      /* S_P2 */
#define OLDSHORTVERSION 10      /* Short packets version 1 */

/* packets sent from remote client to xtrek server */
#define CP_MESSAGE	1		/* send a message */
#define CP_SPEED	2		/* set speed */
#define CP_DIRECTION	3		/* change direction */
#define CP_PHASER	4		/* phaser in a direction */
#define CP_PLASMA	5		/* plasma (in a direction) */
#define CP_TORP		6		/* fire torp in a direction */
#define CP_QUIT		7		/* self destruct */
#define CP_LOGIN	8		/* log in (name, password) */
#define CP_OUTFIT	9		/* outfit to new ship */
#define CP_WAR		10		/* change war status */
#define CP_PRACTR	11		/* create practice robot, transwarp */
#define CP_SHIELD	12		/* raise/lower sheilds */
#define CP_REPAIR	13		/* enter repair mode */
#define CP_ORBIT	14		/* orbit planet/starbase */
#define CP_PLANLOCK	15		/* lock on planet */
#define CP_PLAYLOCK	16		/* lock on player */
#define CP_BOMB		17		/* bomb a planet */
#define CP_BEAM		18		/* beam armies up/down */
#define CP_CLOAK	19		/* cloak on/off */
#define CP_DET_TORPS	20		/* detonate enemy torps */
#define CP_DET_MYTORP	21		/* detonate one of my torps */
#define CP_COPILOT	22		/* toggle copilot mode */
#define CP_REFIT	23		/* refit to different ship type */
#define CP_TRACTOR	24		/* tractor on/off */
#define CP_REPRESS	25		/* pressor on/off */
#define CP_COUP		26		/* coup home planet */
#define CP_SOCKET	27		/* new socket for reconnection */
#define CP_OPTIONS	28		/* send my options to be saved */
#define CP_BYE		29		/* I'm done! */
#define CP_DOCKPERM	30		/* set docking permissions */
#define CP_UPDATES	31		/* set number of usecs per update */
#define CP_RESETSTATS	32		/* reset my stats packet */
#define CP_RESERVED	33		/* for future use */
#define CP_SCAN		34		/* ATM: request for player scan */
#define CP_UDP_REQ	35		/* request UDP on/off */
#define CP_SEQUENCE	36		/* sequence # packet */
#define CP_RSA_KEY	37		/* handles binary verification */
#define CP_PLANET	38		/* cross-check planet info */
#define CP_PING_RESPONSE        42              /* client response */
#define CP_S_REQ	43
#define CP_S_THRS	44
#define CP_S_MESSAGE	45              /* vari. Message Packet */
#define CP_S_RESERVED	46
#define CP_S_DUMMY	47

#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
#define CP_OGGV         50
#endif

/* 51-54 */

#define CP_FEATURE      60

#define MAX_CP_PACKETS	60

#define SOCKVERSION 	4
#define UDPVERSION	10		/* changing this blocks other versions*/

/*
 * These are server --> client packets
 */

struct mesg_spacket { /* SP_MESSAGE py-struct "!bBBB80s" #1 */
    char type;
    u_char m_flags;
    u_char m_recpt;
    u_char m_from;
    char mesg[MSG_LEN];
};

struct plyr_info_spacket { /* SP_PLAYER_INFO py-struct "!bbbb" #2 */
    char type;
    char pnum;
    char shiptype;
    char team;
};

struct kills_spacket { /* SP_KILLS py-struct "!bbxxI" #3 */
    char type;
    char pnum;
    char pad1;
    char pad2;
    u_int kills;	/* where 1234=12.34 kills and 0=0.00 kills */
};

struct player_spacket { /* SP_PLAYER py-struct "!bbBbll" #4 */
    char type;
    char pnum;
    u_char dir;
    char speed;
    LONG x,y;
};

struct torp_info_spacket { /* SP_TORP_INFO py-struct "!bbbxhxx" #5 */
    char  type;
    char  war;		/* mask of teams the torp is hostile toward */
    char  status;	/* new status of this torp, TFREE, TDET, etc... */
    char  pad1;		/* pad needed for cross cpu compatibility */
    short tnum;		/* the torpedo number */
    short pad2;
};

struct torp_spacket { /* SP_TORP py-struct "!bBhll" #6 */
    char  type;
    u_char dir;
    short tnum;
    LONG  x,y;
};

struct phaser_spacket { /* SP_PHASER py-struct "!bbbBlll" #7 */
    char type;
    char pnum;
    char status;	/* PH_HIT, etc... */
    u_char dir;
    LONG x,y;
    LONG target;
};

struct plasma_info_spacket { /* SP_PLASMA_INFO py-struct "!bbbxhxx" #8 */
    char  type;
    char  war;
    char  status;	/* TFREE, TDET, etc... */
    char  pad1;		/* pad needed for cross cpu compatibility */
    short pnum;
    short pad2;
};

struct plasma_spacket { /* SP_PLASMA py-struct "!bxhll" #9 */
    char  type;
    char  pad1;
    short pnum;
    LONG  x,y;
};

struct warning_spacket { /* SP_WARNING py-struct "!bxxx80s" #10 */
    char type;
    char pad1;
    char pad2;
    char pad3;
    char mesg[MSG_LEN];
};

struct motd_spacket { /* SP_MOTD py-struct "!bxxx80s" #11 */
    char type;
    char pad1;
    char pad2;
    char pad3;
    char line[MSG_LEN];
};

struct you_spacket { /* SP_YOU py-struct "!bbbbbbxxIlllhhhh" #12 */
    char type;
    char pnum;		/* Guy needs to know this... */
    char hostile;
    char swar;
    char armies;
    char tractor;	/* ATM - visible tractor (was pad1) */
    char pad2;
    char pad3;
    u_int flags;
    LONG damage;
    LONG shield;
    LONG fuel;
    short etemp;
    short wtemp;
    short whydead;
    short whodead;
};

struct queue_spacket { /* SP_QUEUE py-struct "!bxh" #13 */
    char type;
    char pad1;
    short pos;
};

struct status_spacket { /* SP_STATUS py-struct "!bbxxIIIIIL" #14 */
    char type;
    char tourn;
    char pad1;
    char pad2;
    u_int armsbomb;
    u_int planets;
    u_int kills;
    u_int losses;
    u_int time;
    U_LONG timeprod;
};

struct planet_spacket { /* SP_PLANET py-struct "!bbbbhxxl" #15 */
    char  type;
    char  pnum;
    char  owner;
    char  info;
    short flags;
    short pad2;
    LONG  armies;
};

struct ping_cpacket { /* CP_PING_RESPONSE py-struct "!bBbxll" #42 */
   char                 type;
   u_char	        number;         /* id */
   char                 pingme;         /* if client wants server to ping */
   char                 pad1;

   LONG                 cp_sent;        /* # packets sent to server */
   LONG                 cp_recv;        /* # packets recv from server */
};      /* 12 bytes */

struct ping_spacket { /* SP_PING py-struct "!bBHBBBB" #46 */
   char			type;
   u_char		number;         /* id */
   u_short		lag;            /* delay in ms */

   u_char		tloss_sc;       /* total loss server-client 0-100% */
   u_char		tloss_cs;       /* total loss client-server 0-100% */

   u_char		iloss_sc;       /* inc. loss server-client 0-100% */
   u_char		iloss_cs;       /* inc. loss client-server 0-100% */

};      /* 8 bytes */

struct pickok_spacket { /* SP_PICKOK py-struct "!bbxx" #16 */
    char type;
    char state;         /* 0=no, 1=yes */
    char pad2;
    char pad3;
};

struct login_spacket { /* SP_LOGIN py-struct "!bbxxl96s" #17 */
    char type;
    char accept;	/* 1/0 */
    char pad2;
    char pad3;
    LONG flags;
    char keymap[KEYMAP_LEN];
};

struct flags_spacket { /* SP_FLAGS py-struct "!bbbxI" #18 */
    char type;
    char pnum;		/* whose flags are they? */
    char tractor;	/* ATM - visible tractors */
    char pad2;
    u_int flags;
};

struct mask_spacket { /* SP_MASK py-struct "!bbxx" #19 */
    char type;
    char mask;
    char pad1;
    char pad2;
};

struct pstatus_spacket { /* SP_PSTATUS py-struct "!bbbx" #20 */
    char type;
    char pnum;
    char status;
    char pad1;
};

struct badversion_spacket { /* SP_BADVERSION py-struct "!bbxx" #21 */
    char type;
    char why;
    char pad2;
    char pad3;
};

struct hostile_spacket { /* SP_HOSTILE py-struct "!bbbb" #22 */
    char type;
    char pnum;
    char war;
    char hostile;
};

struct stats_spacket { /* SP_STATS py-struct "!bbxx13l" #23 */
    char type;
    char pnum;
    char pad1;
    char pad2;
    LONG tkills;	/* Tournament kills */
    LONG tlosses;	/* Tournament losses */
    LONG kills;		/* overall */
    LONG losses;	/* overall */
    LONG tticks;	/* ticks of tournament play time */
    LONG tplanets;	/* Tournament planets */
    LONG tarmies;	/* Tournament armies */
    LONG sbkills;	/* Starbase kills */
    LONG sblosses;	/* Starbase losses */
    LONG armies;	/* non-tourn armies */
    LONG planets;	/* non-tourn planets */
    LONG maxkills;	/* max kills as player * 100 */
    LONG sbmaxkills;	/* max kills as sb * 100 */
};

struct plyr_login_spacket { /* SP_PL_LOGIN py-struct "!bbbx16s16s16s" #24 */
    char type;
    char pnum;
    char rank;
    char pad1;
    char name[NAME_LEN];
    char monitor[NAME_LEN];
    char login[NAME_LEN];
};

struct reserved_spacket { /* SP_RESERVED py-struct "!bxxx16s" #25 */
    char type;
    char pad1;
    char pad2;
    char pad3;
    char data[RESERVED_SIZE];
};

struct planet_loc_spacket { /* SP_PLANET_LOC py-struct "!bbxxll16s" #26 */
    char type;
    char pnum;
    char pad2;
    char pad3;
    LONG x;
    LONG y;
    char name[NAME_LEN];
};

struct scan_spacket {		/* ATM */
    char type;		/* SP_SCAN */
    char pnum;
    char success;
    char pad1;
    LONG p_fuel;
    LONG p_armies;
    LONG p_shield;
    LONG p_damage;
    LONG p_etemp;
    LONG p_wtemp;
};

struct udp_reply_spacket { /* SP_UDP_REPLY py-struct '!bbxxi' #28 */
    char type;
    char reply;
    char pad1;
    char pad2;
    int  port;
};

struct sequence_spacket { /* SP_SEQUENCE py-struct '!bBH' #29 */
    char type;
    u_char flag8;
    u_short sequence;
};

struct sc_sequence_spacket {	/* UDP */
    char type;		/* SP_CP_SEQUENCE */
    char pad1;
    u_short sequence;
};

#ifdef RSA
struct rsa_key_spacket {
    char type;          /* SP_RSA_KEY */
    char pad1;
    char pad2;
    char pad3;
    u_char data[KEY_SIZE];
};
#endif

/*
 * These are the client --> server packets
 */

struct mesg_cpacket { /* CP_MESSAGE py-struct "!bBBx80s" #1 */
    char type;
    u_char group;
    u_char indiv;	/* does this break anything? -da */
    char pad1;
    char mesg[MSG_LEN];
};

struct speed_cpacket { /* CP_SPEED py-struct "!bbxx" #2 */
    char type;
    char speed;
    char pad1;
    char pad2;
};

struct dir_cpacket { /* CP_DIRECTION py-struct "!bBxx" #3 */
    char type;
    u_char dir;
    char pad1;
    char pad2;
};

struct phaser_cpacket { /* CP_PHASER py-struct "!bBxx" #4 */
    char type;
    u_char dir;
    char pad1;
    char pad2;
};

struct plasma_cpacket { /* CP_PLASMA py-struct "!bBxx" #5 */
    char type;
    u_char dir;
    char pad1;
    char pad2;
};

struct torp_cpacket { /* CP_TORP py-struct "!bBxx" #6 */
    char type;
    u_char dir;		/* direction to fire torp */
    char pad1;
    char pad2;
};

struct quit_cpacket { /* CP_QUIT py-struct "!bxxx" #7 */
    char type;
    char pad1;
    char pad2;
    char pad3;
};

struct login_cpacket { /* CP_LOGIN py-struct '!bbxx16s16s16s' #8 */
    char type;
    char query;
    char pad2;
    char pad3;
    char name[NAME_LEN];
    char password[NAME_LEN];
    char login[NAME_LEN];
};

struct outfit_cpacket { /* CP_OUTFIT py-struct "!bbbx" #9 */
    char type;
    char team;
    char ship;
    char pad1;
};

struct war_cpacket { /* CP_WAR py-struct "!bbxx" #10 */
    char type;
    char newmask;
    char pad1;
    char pad2;
};

struct practr_cpacket { /* CP_PRACTR py-struct "!bxxx" #11 */
    char type;
    char pad1;
    char pad2;
    char pad3;
};

struct shield_cpacket { /* CP_SHIELD py-struct "!bbxx" #12 */
    char type;
    char state;		/* up/down */
    char pad1;
    char pad2;
};

struct repair_cpacket { /* CP_REPAIR py-struct "!bbxx" #13 */
    char type;
    char state;		/* on/off */
    char pad1;
    char pad2;
};

struct orbit_cpacket { /* CP_ORBIT py-struct "!bbxx" #14 */
    char type;
    char state;		/* on/off */
    char pad1;
    char pad2;
};

struct planlock_cpacket { /* CP_PLANLOCK py-struct "!bbxx" #15 */
    char type;
    char pnum;
    char pad1;
    char pad2;
};

struct playlock_cpacket { /* CP_PLAYLOCK py-struct "!bbxx" #16 */
    char type;
    char pnum;
    char pad1;
    char pad2;
};

struct bomb_cpacket { /* CP_BOMB py-struct "!bbxx" #17 */
    char type;
    char state;
    char pad1;
    char pad2;
};

struct beam_cpacket { /* CP_BEAM py-struct "!bbxx" #18 */
    char type;
    char state;
    char pad1;
    char pad2;
};

struct cloak_cpacket { /* CP_CLOAK py-struct "!bbxx" #19 */
    char type;
    char state;
    char pad1;
    char pad2;
};

struct det_torps_cpacket { /* CP_DET_TORPS py-struct "!bxxx" #20 */
    char type;
    char pad1;
    char pad2;
    char pad3;
};

struct det_mytorp_cpacket { /* CP_DET_MYTORP py-struct "!bxh" #21 */
    char type;
    char pad1;
    short tnum;
};

struct copilot_cpacket { /* CP_COPILOT py-struct "!bbxx" #22 */
    char type;
    char state;
    char pad1;
    char pad2;
};

struct refit_cpacket { /* CP_REFIT py-struct "!bbxx" #23 */
    char type;
    char ship;
    char pad1;
    char pad2;
};

struct tractor_cpacket { /* CP_TRACTOR py-struct "!bbbx" #24 */
    char type;
    char state;
    char pnum;
    char pad2;
};

struct repress_cpacket { /* CP_REPRESS py-struct "!bbbx" #25 */
    char type;
    char state;
    char pnum;
    char pad2;
};

struct coup_cpacket { /* CP_COUP py-struct "!bxxx" #26 */
    char type;
    char pad1;
    char pad2;
    char pad3;
};

struct socket_cpacket { /* CP_SOCKET py-struct "!bbbxI" #27 */
    char type;
    char version;
    char udp_version;	/* was pad2 */
    char pad3;
    u_int socket;
};

struct options_cpacket { /* CP_OPTIONS py-struct "!bxxxI96s" #28 */
    char type;
    char pad1;
    char pad2;
    char pad3;
    u_int flags;
    char keymap[KEYMAP_LEN];
};

struct bye_cpacket { /* CP_BYE py-struct "!bxxx" #29 */
    char type;
    char pad1;
    char pad2;
    char pad3;
};

struct dockperm_cpacket { /* CP_DOCKPERM py-struct "!bbxx" #30 */
    char type;
    char state;
    char pad2;
    char pad3;
};

struct updates_cpacket { /* CP_UPDATES py-struct "!bxxxI" #31 */
    char type;
    char pad1;
    char pad2;
    char pad3;
    u_int usecs;
};

struct resetstats_cpacket { /* CP_RESETSTATS py-struct "!bbxx" #32 */
    char type;
    char verify;	/* 'Y' - just to make sure he meant it */
    char pad2;
    char pad3;
};

struct reserved_cpacket { /* CP_RESERVED py-struct "!bxxx16s16s" #33 */
    char type;
    char pad1;
    char pad2;
    char pad3;
    char data[RESERVED_SIZE];
    char resp[RESERVED_SIZE];
};

struct scan_cpacket { /* CP_SCAN py-struct "!bbxx" #34 */
    char type;
    char pnum;
    char pad1;
    char pad2;
};

struct udp_req_cpacket { /* CP_UDP_REQ py-struct "!bbbxi" #35 */
    char type;
    char request;
    char connmode;	/* respond with port # or just send UDP packet? */
    char pad2;
    int  port;		/* compensate for hosed recvfrom() */
};

struct sequence_cpacket {	/* UDP */
    char type;		/* CP_SEQUENCE */
    char pad1;
    u_short sequence;
};

#ifdef RSA
struct rsa_key_cpacket {
    char type;          /* CP_RSA_KEY */
    char pad1;
    char pad2;
    char pad3;
    u_char global[KEY_SIZE];
    u_char public[KEY_SIZE];
    u_char resp[KEY_SIZE];
};
#endif

struct planet_cpacket
{
    char type;		/* CP_PLANET */
    char pnum;
    char owner;
    char info;
    short flags;
    int armies;
};

struct shortreq_cpacket {       /* CP_S_REQ */
     char type;
     char req;
     char version;
     char pad2;
};

struct threshold_cpacket {      /* CP_S_THRS */
     char type;
     char pad1;
     u_short     thresh;
};

struct shortreply_spacket {     /* SP_S_REPLY */
     char type;
     char repl;
     u_short winside;
     LONG gwidth;
};

struct you_short_spacket {       /* SP_S_YOU */
     char       type;

     char       pnum;
     char       hostile;
     char       swar;

     char       armies;
     char       whydead;
     char       whodead;

     char       pad1;

     u_int	flags;
};

struct youss_spacket {          /* SP_S_YOU_SS */
     char       type;
     char       pad1;

     u_short     damage;
     u_short     shield;
     u_short     fuel;
     u_short     etemp;
     u_short     wtemp;
};

#define VPLANET_SIZE 6

struct planet_s_spacket {       /* body of SP_S_PLANET  */
    char pnum;
    char owner;
    char info;
    u_char armies;       /* more than 255 Armies ? ...  */
    short flags;
};
struct warning_s_spacket {              /* SP_S_WARNING */
    char type;
    u_char  whichmessage;
    char  argument, argument2; /* for phaser  etc ... */
};

struct player_s_spacket {
    char type;          /* SP_S_PLAYER Header */

    char packets; /* How many player-packets are in this packet  ( only the firs
t 6 bits are relevant ) */
    u_char dir;
    char speed;
    LONG x,y;   /* To get the absolute Position */
};
/* S_P2 */
struct player_s2_spacket {
    char type;          /* SP_S_PLAYER Header */

    char packets; /* How many player-packets are in this packet  ( only the first 6 bits are relevant ) */
    u_char dir;
    char speed;
    short x,y;  /* absolute position / 40 */
    LONG flags;   /* 16 playerflags */
};

/* flag header */
struct top_flags { 
     char type;
     unsigned char packets;
     unsigned char numflags; /* How many flag packets */
     unsigned char index;   /* from which index on */ 
     unsigned int tflags;
     unsigned int tflags2;
};

/* The format of the body:
struct player_s_body_spacket {  Body of new Player Packet
        u_char pnum;      0-4 = pnum, 5 local or galactic, 6 = 9. x-bit, 7 9. y-b
it
        u_char speeddir;  0-3 = speed , 4-7 direction of ship
        u_char x;         low 8 bits from X-Pixelcoordinate
        u_char y;         low 8 bits from Y-Pixelcoordinate
};
*/

struct torp_s_spacket {
     char type; /* SP_S_TORP */
     u_char bitset;      /* bit=1 that torp is in packet */
     u_char whichtorps; /* Torpnumber of first torp / 8 */
     u_char data[21]; /* For every torp 2*9 bit coordinates */
};

struct mesg_s_spacket {
    char type;          /* SP_S_MESSAGE */
    u_char m_flags;
    u_char m_recpt;
    u_char m_from;
    u_char length;       /* Length of whole packet */
    char mesg;
    char pad2;
    char pad3;
    char pad[76];
};

struct mesg_s_cpacket {
    char type;          /* CP_S_MESSAGE */
    char group;
    char indiv;
    char length; /* Size of whole packet   */
    char mesg[MSG_LEN];
};

/* S_P2 */
struct kills_s_spacket
  {
    char type;                  /* SP_S_KILLS */
    char pnum;                  /* How many kills in packet */
    unsigned short kills;       /* 6 bit player numer   */
                                /* 10 bit kills*100     */
    unsigned short mkills[MAXPLAYER];
  };

struct phaser_s_spacket
  {
    char type;                  /* SP_S_PHASER */
    u_char status;                /* PH_HIT, etc... */
    u_char pnum;                 /* both bytes are used for more */
    u_char target;               /* look into the code   */
    short x;                    /* x coord /40 */
    short y;                    /* y coord /40 */
    u_char dir;
    char pad1;
    char pad2;
    char pad3;
  };

struct stats_s_spacket
  {
    char type;                  /* SP_S_STATS */
    char pnum;
    unsigned short tplanets;              /* Tournament planets */
    unsigned short tkills;                /* Tournament kills */
    unsigned short tlosses;               /* Tournament losses */
    unsigned short kills;                 /* overall */
    unsigned short losses;                /* overall */
    unsigned int tticks;                /* ticks of tournament play time */
    unsigned int tarmies;               /* Tournament armies */
    unsigned int maxkills;
    unsigned short sbkills;               /* Starbase kills */
    unsigned short sblosses;              /* Starbase losses */
    unsigned short armies;                /* non-tourn armies */
    unsigned short planets;               /* non-tourn planets */
    unsigned int sbmaxkills;            /* max kills as sb * 100 */
  };


#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
struct oggv_cpacket {
   char                 type;   /* CP_OGGV */
   u_char		def;    /* defense 1-100 */
   u_char		targ;   /* target */
};
#endif

/* Server configuration of client */
struct ship_cap_spacket { /* SP_SHIP_CAP py-struct "!bbHHHiiiiiiHHH1sx16s2sH" #39 */
    char	type;		/* screw motd method */
    char	operation;	/* 0 = add/change a ship, 1 = remove a ship */
    u_short	s_type;
    u_short	s_torpspeed;
    u_short	s_phaserrange;
    int		s_maxspeed;
    int		s_maxfuel;
    int		s_maxshield;
    int		s_maxdamage;
    int		s_maxwpntemp;
    int		s_maxegntemp;
    u_short	s_width;
    u_short	s_height;
    u_short	s_maxarmies;
    char	s_letter;
    char	pad2;
    char	s_name[16];
    char	s_desig1;
    char	s_desig2;
    u_short	s_bitmap;
};

struct generic_32_spacket {
    char        type;
    char        pad[31];
};
#define GENERIC_32_LENGTH 32
#define COST_GENERIC_32 (F_sp_generic_32 ? GENERIC_32_LENGTH : 0)
struct generic_32_spacket_a { /* SP_GENERIC_32 py-struct "b1sHH26x" #32 */
    char        type;
    char        version;        /* alphabetic, 0x60 + version */
    u_short     repair_time;    /* server estimate of repair time in seconds */
    u_short     pl_orbit;       /* what planet player orbiting, -1 if none */
    char        pad1[26];
    /* NOTE: this version didn't use network byte order for the shorts */
};
#define GENERIC_32_VERSION_A 1
struct generic_32_spacket_b { /* SP_GENERIC_32 py-struct "!b1sHbHBBsBsBB18x" #32 */
    char        type;
    char        version;        /* alphabetic, 0x60 + version */
    u_short     repair_time;    /* server estimate of repair time, seconds  */
    char        pl_orbit;       /* what planet player orbiting, -1 if none  */
    u_short     gameup;                  /* server status flags             */
    u_char      tournament_teams;        /* what teams are involved         */
    u_char      tournament_age;          /* time since last t-mode start    */
    char        tournament_age_units;    /* units for above, see s2du       */
    u_char      tournament_remain;       /* remaining INL game time         */
    char        tournament_remain_units; /* units for above, see s2du       */
    u_char      starbase_remain;         /* starbase reconstruction, mins   */
    u_char      team_remain;             /* team surrender time, seconds    */
    char        pad1[18];
} __attribute__ ((packed));
#define GENERIC_32_VERSION_B 2
#define GENERIC_32_VERSION GENERIC_32_VERSION_B /* default */

/* SP_GENERIC_32 versioning instructions:

   we start with version 'a', and each time a structure is changed
   increment the version and reduce the pad size, keeping the packet
   the same size ...

   client is entitled to trust fields in struct that were defined at a
   particular version ...

   client is to send CP_FEATURE with SP_GENERIC_32 value 1 for version
   'a', value 2 for version 'b', etc ...

   server is to reply with SP_FEATURE with SP_GENERIC_32 value set to
   the maximum version it supports (not the version requested by the
   client), ...

   server is to send SP_GENERIC_32 packets of the highest version it
   knows about, but no higher than the version the client asks for.
*/

struct flags_all_spacket {
    char        type;           /* SP_FLAGS_ALL */
    char        offset;         /* slot number of first flag */
    int         flags;          /* two bits per slot */
#define FLAGS_ALL_DEAD                   0
#define FLAGS_ALL_CLOAK_ON               1
#define FLAGS_ALL_CLOAK_OFF_SHIELDS_UP   2
#define FLAGS_ALL_CLOAK_OFF_SHIELDS_DOWN 3
};

struct feature_spacket { /* SP_FEATURE py-struct "!bcbbi80s" #60 */
   char                 type;
   char                 feature_type;   /* either 'C' or 'S' */
   char                 arg1,
                        arg2;
   int                  value;
   char                 name[80];
};

struct feature_cpacket { /* CP_FEATURE py-struct "!bcbbi80s" #60 */
   char                 type;
   char                 feature_type;   /* either 'C' or 'S' */
   char                 arg1,
                        arg2;
   int                  value;
   char                 name[80];
};

struct rank_spacket { /* SP_RANK py-struct pending #61 */
    char        type;
    char        rnum;           /* rank number */
    char        rmax;           /* rank number maximum */
    char        pad;
    char        name[NAME_LEN]; /* full rank name */
    int         hours;          /* hundredths of hours required */
    int         ratings;        /* hundredths of ratings required */
    int         offense;        /* hundredths of offense required */
    char        cname[8];       /* short 'curt' rank name */
};

#define LTD_VERSION 'a' /* version for SP_LTD packet */

struct ltd_spacket { /* SP_LTD py-struct pending #62 */
    char        type;
    char        version;
    char        pad[2];
    unsigned int kt;    /* kills total, kills.total */
    unsigned int kmax;  /* kills max, kills.max */
    unsigned int k1;    /* kills first, kills.first */
    unsigned int k1p;   /* kills first potential, kills.first_potential */
    unsigned int k1c;   /* kills first converted, kills.first_converted */
    unsigned int k2;    /* kills second, kills.second */
    unsigned int k2p;   /* kills second potential, kills.second_potential */
    unsigned int k2c;   /* kills second converted, kills.second_converted */
    unsigned int kbp;   /* kills by phaser, kills.phasered */
    unsigned int kbt;   /* kills by torp, kills.torped */
    unsigned int kbs;   /* kills by smack, kills.plasmaed */
    unsigned int dt;    /* deaths total, deaths.total */
    unsigned int dpc;   /* deaths as potential carrier, deaths.potential */
    unsigned int dcc;   /* deaths as converted carrier, deaths.converted */
    unsigned int ddc;   /* deaths as dooshed carrier, deaths.dooshed */
    unsigned int dbp;   /* deaths by phaser, deaths.phasered */
    unsigned int dbt;   /* deaths by torp, deaths.torped */
    unsigned int dbs;   /* deaths by smack, deaths.plasmaed */
    unsigned int acc;   /* actual carriers created, deaths.acc */
    unsigned int ptt;   /* planets taken total, planets.taken */
    unsigned int pdt;   /* planets destroyed total, planets.destroyed */
    unsigned int bpt;   /* bombed planets total, bomb.planets */
    unsigned int bp8;   /* bombed planets <=8, bomb.planets_8 */
    unsigned int bpc;   /* bombed planets core, bomb.planets_core */
    unsigned int bat;   /* bombed armies total, bomb.armies */
    unsigned int ba8;   /* bombed_armies <= 8, bomb.armies_8 */
    unsigned int bac;   /* bombed armies core, bomb.armies_core */
    unsigned int oat;   /* ogged armies total, ogged.armies */
    unsigned int odc;   /* ogged dooshed carrier, ogged.dooshed */
    unsigned int occ;   /* ogged converted carrier, ogged.converted */
    unsigned int opc;   /* ogged potential carrier, ogged.potential */
    unsigned int ogc;   /* ogged bigger carrier, ogged.bigger_ship */
    unsigned int oec;   /* ogged same carrier, ogged.same_ship */
    unsigned int olc;   /* ogger smaller carrier, ogged.smaller_ship */
    unsigned int osba;  /* ogged sb armies, ogged.sb_armies */
    unsigned int ofc;   /* ogged friendly carrier, ogged.friendly */
    unsigned int ofa;   /* ogged friendly armies, ogged.friendly_armies */
    unsigned int at;    /* armies carried total, armies.total */
    unsigned int aa;    /* armies used to attack, armies.attack */
    unsigned int ar;    /* armies used to reinforce, armies.reinforce */
    unsigned int af;    /* armies ferried, armies.ferries */
    unsigned int ak;    /* armies killed, armies.killed */
    unsigned int ct;    /* carries total, carries.total */
    unsigned int cp;    /* carries partial, carries.partial */
    unsigned int cc;    /* carries completed, carries.completed */
    unsigned int ca;    /* carries to attack, carries.attack */
    unsigned int cr;    /* carries to reinforce, carries.reinforce */
    unsigned int cf;    /* carries to ferry, carries.ferries */
    unsigned int tt;    /* ticks total, ticks.total */
    unsigned int tyel;  /* ticks in yellow, ticks.yellow */
    unsigned int tred;  /* ticks in red, ticks.red */
    unsigned int tz0;   /* ticks in zone 0, ticks.zone[0] */
    unsigned int tz1;   /* ticks in zone 1, ticks.zone[1] */
    unsigned int tz2;   /* ticks in zone 2, ticks.zone[2] */
    unsigned int tz3;   /* ticks in zone 3, ticks.zone[3] */
    unsigned int tz4;   /* ticks in zone 4, ticks.zone[4] */
    unsigned int tz5;   /* ticks in zone 5, ticks.zone[5] */
    unsigned int tz6;   /* ticks in zone 6, ticks.zone[6] */
    unsigned int tz7;   /* ticks in zone 7, ticks.zone[7] */
    unsigned int tpc;   /* ticks as potential carrier, ticks.potential */
    unsigned int tcc;   /* ticks as carrier++, ticks.carrier */
    unsigned int tr;    /* ticks in repair, ticks.repair */
    unsigned int dr;    /* damage repaired, damage_repaired */
    unsigned int wpf;   /* weap phaser fired, weapons.phaser.fired */
    unsigned int wph;   /* weap phaser hit, weapons.phaser.hit */
    unsigned int wpdi;  /* weap phaser damage inflicted, weapons.phaser.damage.inflicted */
    unsigned int wpdt;  /* weap phaser damage taken, weapons.phaser.damage.taken */
    unsigned int wtf;   /* weap torp fired, weapons.torps.fired */
    unsigned int wth;   /* weap torp hit, weapons.torps.hit */
    unsigned int wtd;   /* weap torp detted, weapons.torps.detted */
    unsigned int wts;   /* weap torp self detted, weapons.torps.selfdetted */
    unsigned int wtw;   /* weap torp hit wall, weapons.torps.wall */
    unsigned int wtdi;  /* weap torp damage inflicted, weapons.torps.damage.inflicted */
    unsigned int wtdt;  /* weap torp damage taken, weapons.torps.damage.taken */
    unsigned int wsf;   /* weap smack fired, weapons.plasma.fired */
    unsigned int wsh;   /* weap smack hit, weapons.plasma.hit */
    unsigned int wsp;   /* weap smack phasered, weapons.plasma.phasered */
    unsigned int wsw;   /* weap smack hit wall, weapons.plasma.wall */
    unsigned int wsdi;  /* weap smack damage inflicted, weapons.plasma.damage.inflicted */
    unsigned int wsdt;  /* weap smack damage taken, weapons.plasma.damage.taken */
} __attribute__ ((packed));

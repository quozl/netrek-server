/* 
 * Include file for socket I/O xtrek.
 *
 * Kevin P. Smith 1/29/89
 */
#include "copyright2.h"

#ifdef ATM
#define STATUS_TOKEN    "\t@@@"         /* ATM */


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

#endif /*ATM*/

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

#ifdef ATM
/* NOTE: not implemented */
#define SP_SCAN         27              /* ATM: results of player scan */

#define SP_UDP_REPLY    28              /* notify client of UDP status */
#define SP_SEQUENCE     29              /* sequence # packet */
#define SP_SC_SEQUENCE  30              /* this trans is semi-critical info */
#endif /*ATM*/

#ifdef ATM
/* NOTE: not implemented */
#define SP_SCAN         27              /* ATM: results of player scan */

#define SP_UDP_REPLY    28              /* notify client of UDP status */
#define SP_SEQUENCE     29              /* sequence # packet */
#define SP_SC_SEQUENCE  30              /* this trans is semi-critical info */
#endif /*ATM*/

#define SP_PING         46              /* ping packet */


#define SP_MASTER_COMM	47


/* packets sent from remote client to xtrek server */
#define CP_MESSAGE      1		/* send a message */
#define CP_SPEED	2		/* set speed */
#define CP_DIRECTION	3		/* change direction */
#define CP_PHASER	4		/* phaser in a direction */
#define CP_PLASMA	5		/* plasma (in a direction) */
#define CP_TORP		6		/* fire torp in a direction */
#define CP_QUIT		7		/* self destruct */
#define CP_LOGIN	8		/* log in (name, password) */
#define CP_OUTFIT	9		/* outfit to new ship */
#define CP_WAR		10		/* change war status */
#define CP_PRACTR	11		/* create practice robot? */
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

#ifdef ATM
/* NOTE: not implemented. */
#define CP_SCAN         34              /* ATM: request for player scan */

#define CP_UDP_REQ      35              /* request UDP on/off */
#define CP_SEQUENCE     36              /* sequence # packet */
#endif /*ATM*/

#ifdef RSA
#define CP_RSA_KEY      37              /* handles binary verification */
#endif

#define CP_PING_RESPONSE 42             /* client response */
#define CP_S_REQ        43
#define CP_S_THRS       44
#define CP_S_MESSAGE    45              /* vari. Message Packet */
#define CP_S_RESERVED   46
#define CP_S_DUMMY      47

#define CP_OGGV         50

#define CP_MASTER_COMM	51

#define SOCKVERSION	4

#ifdef ATM
#define UDPVERSION      10
#endif /*ATM*/

struct packet_handler {
    int size;
    int (*handler)(char *bufptr);
};

struct mesg_spacket {
    char type;		/* SP_MESSAGE */
    unsigned char m_flags;
    unsigned char m_recpt;
    unsigned char m_from;
    char mesg[80];
};

struct plyr_info_spacket {
    char type;		/* SP_PLAYER_INFO */
    char pnum;
    char shiptype;	
    char team;
};

struct plyr_login_spacket {
    char type;		/* SP_PL_LOGIN */
    char pnum;
    char rank;
    char pad1;
    char name[16];
    char monitor[16];
    char login[16];
};

struct hostile_spacket {
    char type;		/* SP_HOSTILE */
    char pnum;
    char war;
    char hostile;
};

struct stats_spacket {
    char type;		/* SP_STATS */
    char pnum;
    char pad1;
    char pad2;
    long tkills;	/* Tournament kills */
    long tlosses;	/* Tournament losses */
    long kills;		/* overall */
    long losses;	/* overall */
    long tticks;	/* ticks of tournament play time */
    long tplanets;	/* Tournament planets */
    long tarmies;	/* Tournament armies */
    long sbkills;	/* Starbase kills */
    long sblosses;	/* Starbase losses */
    long armies;	/* non-tourn armies */
    long planets;	/* non-tourn planets */
    long maxkills;	/* max kills as player * 100 */
    long sbmaxkills;	/* max kills as sb * 100 */
};

struct flags_spacket {
    char type;		/* SP_FLAGS */
    char pnum;		/* whose flags are they? */
    char pad1;
    char pad2;
    unsigned flags;
};

struct kills_spacket {
    char type;		/* SP_KILLS */
    char pnum;
    char pad1;
    char pad2;
    unsigned kills;	/* where 1234=12.34 kills and 0=0.00 kills */
};

struct player_spacket {
    char type;		/* SP_PLAYER */
    char pnum;		
    unsigned char dir;
    char speed;
    long x,y;
};

struct torp_info_spacket {
    char  type;		/* SP_TORP_INFO */
    char  war;		
    char  status;	/* TFREE, TDET, etc... */
    char  pad1;		/* pad needed for cross cpu compatibility */
    short tnum;		
    short pad2;
};

struct torp_spacket {
    char  type;		/* SP_TORP */
    unsigned char dir;
    short tnum;
    long  x,y;
};

struct phaser_spacket {
    char type;		/* SP_PHASER */
    char pnum;
    char status;	/* PH_HIT, etc... */
    unsigned char dir;
    long x,y;
    long target;
};

struct you_spacket {
    char type;		/* SP_YOU */
    char pnum;		/* Guy needs to know this... */
    char hostile;
    char swar;
    char armies;
    char pad1;
    char pad2;
    char pad3;
    unsigned flags;
    long damage;
    long shield;
    long fuel;
    short etemp;
    short wtemp;
    short whydead;
    short whodead;
};

struct status_spacket {
    char type;		/* SP_STATUS */
    char tourn;
    char pad1;
    char pad2;
    unsigned armsbomb;
    unsigned planets;
    unsigned kills;
    unsigned losses;
    unsigned time;
    unsigned long timeprod;
};

struct warning_spacket {
    char type;		/* SP_WARNING */
    char pad1;
    char pad2;
    char pad3;
    char mesg[80];
};

struct planet_spacket {
    char  type;		/* SP_PLANET */
    char  pnum;
    char  owner;
    char  info;		
    short flags;
    short pad2;
    long  armies;
};

struct torp_cpacket {
    char type;		/* CP_TORP */
    unsigned char dir;		/* direction to fire torp */
    char pad1;
    char pad2;
};

struct phaser_cpacket {
    char type;		/* CP_PHASER */
    unsigned char dir;
    char pad1;
    char pad2;
};

struct speed_cpacket {
    char type;		/* CP_SPEED */
    char speed;		
    char pad1;
    char pad2;
};

struct dir_cpacket {
    char type;		/* CP_DIRECTION */
    unsigned char dir;
    char pad1;
    char pad2;
};

struct shield_cpacket {
    char type;		/* CP_SHIELD */
    char state;		/* up/down */
    char pad1;
    char pad2;
};

struct repair_cpacket {
    char type;		/* CP_REPAIR */
    char state;		/* on/off */
    char pad1;
    char pad2;
};

struct orbit_cpacket {
    char type;		/* CP_ORBIT */
    char state;		/* on/off */
    char pad1;
    char pad2;
};

struct practr_cpacket {
    char type;		/* CP_PRACTR */
    char pad1;
    char pad2;
    char pad3;
};

struct bomb_cpacket {
    char type;		/* CP_BOMB */
    char state;
    char pad1;
    char pad2;
};

struct beam_cpacket {
    char type;		/* CP_BEAM */
    char state;
    char pad1; 
    char pad2;
};

struct cloak_cpacket {
    char type;		/* CP_CLOAK */
    char state;		
    char pad1;
    char pad2;
};

struct det_torps_cpacket {
    char type;		/* CP_DET_TORPS */
    char pad1;
    char pad2;
    char pad3;
};

struct copilot_cpacket {
    char type;		/* CP_COPLIOT */
    char state;
    char pad1;
    char pad2;
};

struct queue_spacket {
    char type;		/* SP_QUEUE */
    char pad1;
    short pos;
};

struct outfit_cpacket {
    char type;		/* CP_OUTFIT */
    char team;
    char ship;
    char pad1;
};

struct pickok_spacket {
    char type;		/* SP_PICKOK */
    char state;
    char pad2;
    char pad3;
};

struct login_cpacket {
    char type;		/* CP_LOGIN */
    char query;
    char pad2;
    char pad3;
    char name[16];
    char password[16];
    char login[16];
};

struct login_spacket {
    char type;		/* SP_LOGIN */
    char accept;	/* 1/0 */
    char pad2;
    char pad3;
    long flags;
    char keymap[96];
};

struct tractor_cpacket {
    char type;		/* CP_TRACTOR */
    char state;
    char pnum;
    char pad2;
};

struct repress_cpacket {
    char type;		/* CP_REPRESS */
    char state;
    char pnum;
    char pad2;
};

struct det_mytorp_cpacket {
    char type;		/* CP_DET_MYTORP */
    char pad1;
    short tnum;
};

struct war_cpacket {
    char type;		/* CP_WAR */
    char newmask;
    char pad1;
    char pad2;
};

struct refit_cpacket {
    char type;		/* CP_REFIT */
    char ship;
    char pad1;
    char pad2;
};

struct plasma_cpacket {
    char type;		/* CP_PLASMA */
    unsigned char dir;
    char pad1;
    char pad2;
};

struct plasma_info_spacket {
    char  type;		/* SP_PLASMA_INFO */
    char  war;		
    char  status;	/* TFREE, TDET, etc... */
    char  pad1;		/* pad needed for cross cpu compatibility */
    short pnum;		
    short pad2;
};

struct plasma_spacket {
    char  type;		/* SP_PLASMA */
    char  pad1;
    short pnum;
    long  x,y;
};

struct playlock_cpacket {
    char type;		/* CP_PLAYLOCK */
    char pnum;
    char pad1;
    char pad2;
};

struct planlock_cpacket {
    char type;		/* CP_PLANLOCK */
    char pnum;
    char pad1;
    char pad2;
};

struct coup_cpacket {
    char type;		/* CP_COUP */
    char pad1;
    char pad2;
    char pad3;
};

struct pstatus_spacket {
    char type;		/* SP_PSTATUS */
    char pnum;
    char status;
    char pad1;
};

struct motd_spacket {
    char type;		/* SP_MOTD */
    char pad1;
    char pad2;
    char pad3;
    char line[80];
};

struct quit_cpacket {
    char type;		/* CP_QUIT */
    char pad1;
    char pad2;
    char pad3;
};

struct mesg_cpacket {
    char type;		/* CP_MESSAGE */
    char group;
    char indiv;
    char pad1;
    char mesg[80];
};

struct mask_spacket {
    char type;		/* SP_MASK */
    char mask;
    char pad1;
    char pad2;
};

struct socket_cpacket {
    char type;		/* CP_SOCKET */
    char version;
#ifdef ATM
    char udp_version;   /* was pad2 */
#else
    char pad2;
#endif /*ATM*/
    char pad3;
    unsigned socket;
};

struct options_cpacket {
    char type;		/* CP_OPTIONS */
    char pad1;
    char pad2;
    char pad3;
    unsigned flags;
    char keymap[96];
};

struct bye_cpacket {
    char type;		/* CP_BYE */
    char pad1;
    char pad2;
    char pad3;
};

struct badversion_spacket {
    char type;		/* SP_BADVERSION */
    char why;
    char pad2;
    char pad3;
};

struct dockperm_cpacket {
    char type;		/* CP_DOCKPERM */
    char state;
    char pad2;
    char pad3;
};

struct updates_cpacket {
    char type;		/* CP_UPDATES */
    char pad1;
    char pad2;
    char pad3;
    unsigned usecs;
};

struct resetstats_cpacket {
    char type;		/* CP_RESETSTATS */
    char verify;	/* 'Y' - just to make sure he meant it */
    char pad2;
    char pad3;
};

struct reserved_spacket {
    char type;		/* SP_RESERVED */
    char pad1;
    char pad2;
    char pad3;
    char data[16];
};

struct reserved_cpacket {
    char type;		/* CP_RESERVED */
    char pad1;
    char pad2;
    char pad3;
    char data[16];
    char resp[16];
};

#ifdef ATM
struct udp_req_cpacket {        /* UDP */
    char type;          /* CP_UDP_REQ */
    char request;
    char connmode;      /* respond with port # or just send UDP packet? */
    char pad2;
    int  port;          /* compensate for hosed recvfrom() */
};

struct sequence_cpacket {       /* UDP */
    char type;          /* CP_SEQUENCE */
    char pad1;
    unsigned short sequence;
};

struct ping_cpacket {
   char                 type;           /* CP_PING_RESPONSE */
   unsigned char        number;         /* id */
   char                 pingme;         /* if client wants server to ping */
   char			pad1;

   long                 cp_sent;        /* # packets sent to server */
   long                 cp_recv;        /* # packets recv from server */
};

struct ping_spacket {
   char                 type;           /* SP_PING */
   unsigned char        number;         /* id */
   unsigned short       lag;            /* delay in ms */

   unsigned char        tloss_sc;       /* total loss server-client 0-100% */
   unsigned char        tloss_cs;       /* total loss client-server 0-100% */

   unsigned char        iloss_sc;       /* inc. loss server-client 0-100% */
   unsigned char        iloss_cs;       /* inc. loss client-server 0-100% */
};

struct shortreq_cpacket {	/* CP_S_REQ */
     char type;
     char req;
     char pad1,pad2;
};

struct sc_sequence_spacket {    /* UDP */
    char type;          /* SP_CP_SEQUENCE */
    char pad1;
    unsigned short sequence;
};

struct shortreply_spacket {	/* SP_S_REPLY */
     char type;
     char repl;
     char pad1,pad2;
};

struct youshort_spacket {	/* SP_S_YOU */
     char  	type;

     char	pnum;
     char	hostile;
     char	swar;

     char	armies;
     char	whydead;
     char	whodead;

     char	pad1;

     unsigned	flags;
};

struct youss_spacket {		/* SP_S_YOU_SS */
     char  	type;
     char	pad1;

     unsigned short	damage;
     unsigned short	shield;
     unsigned short	fuel;
     unsigned short	etemp;
     unsigned short	wtemp;
};

struct udp_reply_spacket {      /* UDP */
    char type;          /* SP_UDP_REPLY */
    char reply;
    char pad1;
    char pad2;
    int  port;
};

struct sequence_spacket {       /* UDP */
    char type;          /* SP_SEQUENCE */
    char pad1;
    unsigned short sequence;
};
#endif /*ATM*/


struct planet_loc_spacket {
    char type;		/* SP_PLANET_LOC */
    char pnum;
    char pad2;
    char pad3;
    long x;
    long y;
    char name[16];
};

struct mastercomm_spacket {     /* master command */
    char type;
};

struct oggv_cpacket {
   char                 type;   /* CP_OGGV */
   unsigned char	def;	/* defense 1-100 */
   unsigned char        targ;   /* target */
};

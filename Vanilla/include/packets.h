/* $Id: packets.h,v 1.2 2005/03/21 10:17:16 quozl Exp $
 */

/* 
 * Include file for socket I/O xtrek.
 *
 * Kevin P. Smith 1/29/89
 */
#include "copyright2.h"

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
#define SP_S_REPLY      40              /* reply to send-short request */
#define SP_S_MESSAGE    41              /* var. Message Packet */
#define SP_S_WARNING    42              /* Warnings with 4  Bytes */
#define SP_S_YOU        43              /* hostile,armies,whydead,etc .. */
#define SP_S_YOU_SS     44              /* your ship status */
#define SP_S_PLAYER     45              /* variable length player packet */
#ifdef PING
#define SP_PING         46              /* ping packet */
#endif

#define SP_GENERIC_32	32		/* 32 byte generic, see struct */
#define SP_SHIP_CAP	39		/* Handles server ship mods */

#define SP_S_TORP       47              /* variable length torp packet */
#define SP_S_TORP_INFO  48              /* SP_S_TORP with TorpInfo */
#define SP_S_8_TORP     49              /* optimized SP_S_TORP */
#define SP_S_PLANET     50              /* see SP_PLANET */
/* S_P2 */
#define SP_S_SEQUENCE   56	/* SP_SEQUENCE for compressed packets */
#define SP_S_PHASER     57      /* see struct */
#define SP_S_KILLS      58      /* # of kills player have */
#define SP_S_STATS      59      /* see SP_STATS */

/* variable length packets */
#define VPLAYER_SIZE    4
#define TOTAL_SPACKETS  62
#define SP2SHORTVERSION 11      /* other number blocks, like UDP Version */
#define OLDSHORTVERSION 10      /* S_P2 */

/* 51-54 unused */

#ifdef FEATURE_PACKETS
#define SP_FEATURE      60
#endif

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
#define CP_SCAN		34		/* ATM: request for player scan */
#define CP_UDP_REQ	35		/* request UDP on/off */
#define CP_SEQUENCE	36		/* sequence # packet */
#define CP_RSA_KEY	37		/* handles binary verification */
#define CP_PLANET	38		/* cross-check planet info */

#ifdef PING
#define CP_PING_RESPONSE        42              /* client response */
#endif

#define CP_S_REQ	43
#define CP_S_THRS	44
#define CP_S_MESSAGE	45              /* vari. Message Packet */
#define CP_S_RESERVED	46
#define CP_S_DUMMY	47

#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
#define CP_OGGV         50
#endif

/* 51-54 */

#ifdef FEATURE_PACKETS
#define CP_FEATURE      60
#endif

#define MAX_CP_PACKETS	60

#define SOCKVERSION 	4
#define UDPVERSION	10		/* changing this blocks other versions*/

/*
 * These are server --> client packets
 */

struct mesg_spacket {
    char type;		/* SP_MESSAGE */
    u_char m_flags;
    u_char m_recpt;
    u_char m_from;
    char mesg[MSG_LEN];
};

struct plyr_info_spacket {
    char type;		/* SP_PLAYER_INFO */
    char pnum;
    char shiptype;	
    char team;
};

struct kills_spacket {
    char type;		/* SP_KILLS */
    char pnum;
    char pad1;
    char pad2;
    u_int kills;	/* where 1234=12.34 kills and 0=0.00 kills */
};

struct player_spacket {
    char type;		/* SP_PLAYER */
    char pnum;		
    u_char dir;
    char speed;
    LONG x,y;
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
    u_char dir;
    short tnum;
    LONG  x,y;
};

struct phaser_spacket {
    char type;		/* SP_PHASER */
    char pnum;
    char status;	/* PH_HIT, etc... */
    u_char dir;
    LONG x,y;
    LONG target;
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
    LONG  x,y;
};

struct warning_spacket {
    char type;		/* SP_WARNING */
    char pad1;
    char pad2;
    char pad3;
    char mesg[MSG_LEN];
};

struct motd_spacket {
    char type;		/* SP_MOTD */
    char pad1;
    char pad2;
    char pad3;
    char line[MSG_LEN];
};

struct you_spacket {
    char type;		/* SP_YOU */
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

struct queue_spacket {
    char type;		/* SP_QUEUE */
    char pad1;
    short pos;
};

struct status_spacket {
    char type;		/* SP_STATUS */
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

struct planet_spacket {
    char  type;		/* SP_PLANET */
    char  pnum;
    char  owner;
    char  info;		
    short flags;
    short pad2;
    LONG  armies;
};

#ifdef PING
struct ping_cpacket {
   char                 type;           /* CP_PING_RESPONSE */
   u_char	        number;         /* id */
   char                 pingme;         /* if client wants server to ping */
   char                 pad1;

   LONG                 cp_sent;        /* # packets sent to server */
   LONG                 cp_recv;        /* # packets recv from server */
};      /* 12 bytes */

struct ping_spacket {
   char			type;           /* SP_PING */
   u_char		number;         /* id */
   u_short		lag;            /* delay in ms */

   u_char		tloss_sc;       /* total loss server-client 0-100% */
   u_char		tloss_cs;       /* total loss client-server 0-100% */

   u_char		iloss_sc;       /* inc. loss server-client 0-100% */
   u_char		iloss_cs;       /* inc. loss client-server 0-100% */

};      /* 8 bytes */
#endif


struct pickok_spacket {
    char type;		/* SP_PICKOK */
    char state;
    char pad2;
    char pad3;
};

struct login_spacket {
    char type;		/* SP_LOGIN */
    char accept;	/* 1/0 */
    char pad2;
    char pad3;
    LONG flags;
    char keymap[KEYMAP_LEN];
};

struct flags_spacket {
    char type;		/* SP_FLAGS */
    char pnum;		/* whose flags are they? */
    char tractor;	/* ATM - visible tractors */
    char pad2;
    u_int flags;
};

struct mask_spacket {
    char type;		/* SP_MASK */
    char mask;
    char pad1;
    char pad2;
};

struct pstatus_spacket {
    char type;		/* SP_PSTATUS */
    char pnum;
    char status;
    char pad1;
};

struct badversion_spacket {
    char type;		/* SP_BADVERSION */
    char why;
    char pad2;
    char pad3;
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

struct plyr_login_spacket {
    char type;		/* SP_PL_LOGIN */
    char pnum;
    char rank;
    char pad1;
    char name[NAME_LEN];
    char monitor[NAME_LEN];
    char login[NAME_LEN];
};

struct reserved_spacket {
    char type;		/* SP_RESERVED */
    char pad1;
    char pad2;
    char pad3;
    char data[RESERVED_SIZE];
};

struct planet_loc_spacket {
    char type;		/* SP_PLANET_LOC */
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

struct udp_reply_spacket {	/* UDP */
    char type;		/* SP_UDP_REPLY */
    char reply;
    char pad1;
    char pad2;
    int  port;
};

struct sequence_spacket {	/* UDP */
    char type;		/* SP_SEQUENCE */
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

struct mesg_cpacket {
    char type;		/* CP_MESSAGE */
    u_char group;
    u_char indiv;	/* does this break anything? -da */
    char pad1;
    char mesg[MSG_LEN];
};

struct speed_cpacket {
    char type;		/* CP_SPEED */
    char speed;		
    char pad1;
    char pad2;
};

struct dir_cpacket {
    char type;		/* CP_DIRECTION */
    u_char dir;
    char pad1;
    char pad2;
};

struct phaser_cpacket {
    char type;		/* CP_PHASER */
    u_char dir;
    char pad1;
    char pad2;
};

struct plasma_cpacket {
    char type;		/* CP_PLASMA */
    u_char dir;
    char pad1;
    char pad2;
};

struct torp_cpacket {
    char type;		/* CP_TORP */
    u_char dir;		/* direction to fire torp */
    char pad1;
    char pad2;
};

struct quit_cpacket {
    char type;		/* CP_QUIT */
    char pad1;
    char pad2;
    char pad3;
};

struct login_cpacket {
    char type;		/* CP_LOGIN */
    char query;
    char pad2;
    char pad3;
    char name[NAME_LEN];
    char password[NAME_LEN];
    char login[NAME_LEN];
};

struct outfit_cpacket {
    char type;		/* CP_OUTFIT */
    char team;
    char ship;
    char pad1;
};

struct war_cpacket {
    char type;		/* CP_WAR */
    char newmask;
    char pad1;
    char pad2;
};

struct practr_cpacket {
    char type;		/* CP_PRACTR */
    char pad1;
    char pad2;
    char pad3;
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

struct planet_cpacket
{
    char type;		/* CP_PLANET */
    char pnum;
    char owner;
    char info;
    short flags;
    int armies;
};

struct planlock_cpacket {
    char type;		/* CP_PLANLOCK */
    char pnum;
    char pad1;
    char pad2;
};

struct playlock_cpacket {
    char type;		/* CP_PLAYLOCK */
    char pnum;
    char pad1;
    char pad2;
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

struct det_mytorp_cpacket {
    char type;		/* CP_DET_MYTORP */
    char pad1;
    short tnum;
};

struct copilot_cpacket {
    char type;		/* CP_COPLIOT */
    char state;
    char pad1;
    char pad2;
};

struct refit_cpacket {
    char type;		/* CP_REFIT */
    char ship;
    char pad1;
    char pad2;
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

struct coup_cpacket {
    char type;		/* CP_COUP */
    char pad1;
    char pad2;
    char pad3;
};

struct socket_cpacket {
    char type;		/* CP_SOCKET */
    char version;
    char udp_version;	/* was pad2 */
    char pad3;
    u_int socket;
};

struct options_cpacket {
    char type;		/* CP_OPTIONS */
    char pad1;
    char pad2;
    char pad3;
    u_int flags;
    char keymap[KEYMAP_LEN];
};

struct bye_cpacket {
    char type;		/* CP_BYE */
    char pad1;
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
    u_int usecs;
};

struct resetstats_cpacket {
    char type;		/* CP_RESETSTATS */
    char verify;	/* 'Y' - just to make sure he meant it */
    char pad2;
    char pad3;
};

struct reserved_cpacket {
    char type;		/* CP_RESERVED */
    char pad1;
    char pad2;
    char pad3;
    char data[RESERVED_SIZE];
    char resp[RESERVED_SIZE];
};

struct scan_cpacket {		/* ATM */
    char type;		/* CP_SCAN */
    char pnum;
    char pad1;
    char pad2;
};

struct udp_req_cpacket {	/* UDP */
    char type;		/* CP_UDP_REQ */
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
    char type;          /* CP_MESSAGE */
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

struct ship_cap_spacket {	/* Server configuration of client */
    char	type;		/* screw motd method */
    char	operation;	/* 0 = add/change a ship, 1 = remove a ship */
    u_short	s_type;		/* SP_SHIP_CAP */
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
    char	type;		/* SP_GENERIC_32 */
    char	version;	/* alphabetic */
    short	repair_time;	/* server estimate of repair time in seconds */
    short	pl_orbit;	/* what planet player orbiting, -1 if none */
    char	pad1[25];	/* TODO: consider using a union */
#define GENERIC_32_VERSION 'a'
#define GENERIC_32_LENGTH 32
};

/* versioning instructions: we start with version 'a', and each time a
   field is added increment the version and reduce the pad size,
   keeping the packet the same size ... client is entitled to trust
   fields in struct that were defined at a particular version. */

/*
    packet.type = SP_GENERIC_32;
    packet.version = GENERIC_32_VERSION;
    if (sizeof(struct generic_32_spacket) != GENERIC_32_LENGTH) abort();
*/

#ifdef FEATURE_PACKETS

struct feature_spacket {                /* SP_FEATURE */
   char                 type;
   char                 feature_type;   /* either 'C' or 'S' */
   char                 arg1,
                        arg2;

   int                  value;

   char                 name[80];
};
#endif /* FEATURE_PACKETS */

#ifdef FEATURE_PACKETS
struct feature_cpacket {                /* CP_FEATURE */
   char                 type;
   char                 feature_type;   /* either 'C' or 'S' */
   char                 arg1,
                        arg2;
   int                  value;
   char                 name[80];
};
#endif /* FEATURE_PACKETS */


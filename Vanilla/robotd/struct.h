/*
 * struct.h for the client of an xtrek socket protocol.
 *
 * Most of the unneeded stuff in the structures has been thrown away.
 */
#include "copyright.h"

struct status {
    unsigned char tourn;	/* Tournament mode? */
	    /* These stats only updated during tournament mode */
    unsigned int armsbomb, planets, kills, losses, time;
	    /* Use long for this, so it never wraps */
    unsigned long timeprod;
};

#define PFREE 0
#define POUTFIT 1
#define PALIVE 2
#define PEXPLODE 3
#define PDEAD 4

#define PFSHIELD	0x0001
#define PFREPAIR	0x0002
#define PFBOMB		0x0004
#define PFORBIT		0x0008
#define PFCLOAK		0x0010
#define PFWEP		0x0020
#define PFENG		0x0040
#define PFROBOT		0x0080
#define PFBEAMUP	0x0100
#define PFBEAMDOWN	0x0200
#define PFSELFDEST	0x0400
#define PFGREEN		0x0800
#define PFYELLOW	0x1000
#define PFRED		0x2000
#define PFPLOCK		0x4000		/* Locked on a player */
#define PFPLLOCK	0x8000		/* Locked on a planet */
#define PFCOPILOT	0x10000		/* Allow copilots */
#define PFWAR		0x20000		/* computer reprogramming for war */
#define PFPRACTR	0x40000		/* practice type robot (no kills) */
#define PFDOCK          0x80000         /* true if docked to a starbase */
#define PFREFIT         0x100000        /* true if about to refit */
#define PFREFITTING	0x200000	/* true if currently refitting */
#define PFTRACT  	0x400000	/* tractor beam activated */
#define PFPRESS  	0x800000	/* pressor beam activated */
#define PFDOCKOK	0x1000000	/* docking permission */
#define PFBPROBOT	0x80000000      /* OggV Packet to ID other bots */

#define KQUIT		0x01		/* Player quit */
#define KTORP		0x02		/* killed by torp */
#define KPHASER		0x03		/* killed by phaser */
#define KPLANET		0x04		/* killed by planet */
#define KSHIP		0x05		/* killed by other ship */
#define KDAEMON		0x06		/* killed by dying daemon */
#define KWINNER		0x07		/* killed by a winner */
#define KGHOST		0x08		/* killed because a ghost */
#define KGENOCIDE	0x09		/* killed by genocide */
#define KPROVIDENCE	0x0a		/* killed by a hacker */
#define KPLASMA         0x0b            /* killed by a plasma torpedo */

#define NUM_TYPES 7
#define SCOUT 0
#define DESTROYER 1
#define CRUISER 2
#define BATTLESHIP 3
#define ASSAULT 4
#define STARBASE 5
#define GALAXY 6

#ifdef nodef
struct ship {
    short s_phaserdamage;
    int s_maxspeed;
    int s_maxfuel;
    int s_maxshield;
    int s_maxdamage;
    int s_maxegntemp;
    int s_maxwpntemp;
    short s_maxarmies;
    short s_width;
    short s_height;
    short s_type;
    int s_torpspeed;
};
#endif /* nodef */
struct ship {
    int s_turns;
    short s_accs;
    short s_torpdamage;
    short s_phaserdamage;
    short s_phaserfuse;
    short s_plasmadamage;
    short s_torpspeed;
    short s_torpfuse;
    short s_torpturns;
    short s_plasmaspeed;
    short s_plasmafuse;
    short s_plasmaturns;
    int s_maxspeed;
    short s_repair;
    int s_maxfuel;
    short s_torpcost;
    short s_plasmacost;
    short s_phasercost;
    short s_detcost;
    short s_warpcost;
    short s_cloakcost;
    short s_recharge;
    int s_accint;
    int s_decint;
    int s_maxshield;
    int s_maxdamage;
    int s_maxegntemp;
    int s_maxwpntemp;
    short s_egncoolrate;
    short s_wpncoolrate;
    short s_maxarmies;
    short s_width;
    short s_height;
    short s_type;
    short s_mass;        /* to guage affect of tractor beams */
    short s_tractstr;    /* Strength of tractor beam */
    float s_tractrng;
};

struct stats {
    double st_maxkills;		/* max kills ever */
    int st_kills;		/* how many kills */
    int st_losses;		/* times killed */
    int st_armsbomb;		/* armies bombed */
    int st_planets;		/* planets conquered */
    int st_ticks;		/* Ticks I've been in game */
    int st_tkills;		/* Kills in tournament play */
    int st_tlosses;		/* Losses in tournament play */
    int st_tarmsbomb;		/* Tournament armies bombed */
    int st_tplanets;		/* Tournament planets conquered */
    int st_tticks;		/* Tournament ticks */
				/* SB stats are entirely separate */
    int st_sbkills;		/* Kills as starbase */
    int st_sblosses;		/* Losses as starbase */
    int st_sbticks;		/* Time as starbase */
    double st_sbmaxkills;       /* Max kills as starbase */
    long st_lastlogin;		/* Last time this player was played */
    int st_flags;		/* Misc option flags */
    char st_keymap[96];		/* keymap for this player */
    int st_rank;		/* Ranking of the player */
};

#define ST_MAPMODE      1
#define ST_NAMEMODE     2
#define ST_SHOWSHIELDS  4
#define ST_KEEPPEACE    8
#define ST_SHOWLOCAL    16      /* two bits for these two */
#define ST_SHOWGLOBAL   64

struct player {
    int p_no;
    int p_updates;		/* Number of updates ship has survived */
    int p_status;		/* Player status */
    unsigned int p_flags;	/* Player flags */
    char p_name[16];
    char p_login[16];
    char p_monitor[16];		/* Monitor being played on */
    char p_mapchars[3];		/* Cache for map window image */
    struct ship p_ship;		/* Personal ship statistics */
    int p_x;
    int p_y;
    unsigned char p_dir;	/* Real direction */
    unsigned char p_desdir;	/* desired direction */
    int p_subdir;		/* fraction direction change */
    int p_speed;		/* Real speed */
    short p_desspeed;		/* Desired speed */
    int p_subspeed;		/* Fractional speed */
    short p_team;			/* Team I'm on */
    int p_damage;		/* Current damage */
    int p_subdamage;		/* Fractional damage repair */
    int p_shield;		/* Current shield power */
    int p_subshield;		/* Fractional shield recharge */
    short p_cloakphase;		/* Drawing stage of cloaking engage/disengage. */
    int p_lcloakupd;
    short p_ntorp;		/* Number of torps flying */
    short p_nplasmatorp;        /* Number of plasma torps active */
    char p_hostile;		/* Who my torps will hurt */
    char p_swar;		/* Who am I at sticky war with */
    float p_kills;		/* Enemies killed */
    short p_planet;		/* Planet orbiting or locked onto */
    short p_playerl;		/* Player locked onto */
    short p_armies;	
    int p_fuel;
    short p_explode;		/* Keeps track of final explosion */
    int p_etemp;
    short p_etime;
    int p_wtemp;
    short p_wtime;
    short p_whydead;		/* Tells you why you died */
    short p_whodead;		/* Tells you who killed you */
    struct stats p_stats;	/* player statistics */
    short p_genoplanets;	/* planets taken since last genocide */
    short p_genoarmsbomb;	/* armies bombed since last genocide */
    short p_planets;		/* planets taken this game */
    short p_armsbomb;		/* armies bombed this game */
    int p_ghostbuster;
    short p_tractor;		/* What player is in tractor lock */
    int p_pos;			/* My position in the player file */
};

struct statentry {
    char name[16], password[16];
    struct stats stats;
};

/* Torpedo states */

#define TFREE 0
#define TMOVE 1
#define TEXPLODE 2
#define TDET 3
#define TOFF 4
#define TSTRAIGHT 5		/* Non-wobbling torp */

struct torp {
    int t_no;
    int t_status;		/* State information */
    int t_owner;		
    int t_x;
    int t_y;
    unsigned char t_dir;	/* direction */
    short t_turns; 		/* rate of change of direction if tracking */
    int t_damage;		/* damage for direct hit */
    int t_speed;		/* Moving speed */
    int t_fuse;			/* Life left in current state */
    char t_war;			/* enemies */
    char t_team;		/* launching team */
    char t_whodet;		/* who detonated... */

    /* NEW */
    char t_shoulddet;
};

/* Plasma Torpedo states */

#define PTFREE 0
#define PTMOVE 1
#define PTEXPLODE 2
#define PTDET 3

struct plasmatorp {
    int pt_no;
    int pt_status;		/* State information */
    int pt_owner;
    int pt_x;
    int pt_y;
    unsigned char pt_dir;	/* direction */
    short pt_turns;             /* ticks turned per cycle */
    int pt_damage;		/* damage for direct hit */
    int pt_speed;		/* Moving speed */
    int pt_fuse;	        /* Life left in current state */
    char pt_war;		/* enemies */
    char pt_team;		/* launching team */
};

#define PHFREE 0x00
#define PHHIT  0x01	/* When it hits a person */
#define PHMISS 0x02
#define PHHIT2 0x04	/* When it hits a photon */

struct phaser {
    int ph_status;		/* What it's up to */
    unsigned char ph_dir;	/* direction */
    int ph_target;		/* Who's being hit (for drawing) */
    int ph_x, ph_y;		/* For when it hits a torp */
    int ph_fuse;		/* Life left for drawing */
    int ph_damage;		/* Damage inflicted on victim */
};

/* An important note concerning planets:  The game assumes that
    the planets are in a 'known' order.  Ten planets per team,
    the first being the home planet.
*/

/* the lower bits represent the original owning team */
#define PLREPAIR 0x010
#define PLFUEL 0x020
#define PLAGRI 0x040		
#define PLREDRAW 0x080		/* Player close for redraw */
#define PLHOME 0x100		/* home planet for a given team */
#define PLCOUP 0x200		/* Coup has occured */
#define PLCHEAP 0x400		/* Planet was taken from undefended team */

struct planet {
    int pl_no;
    int pl_flags;		/* State information */
    int pl_owner;
    int pl_x;
    int pl_y;
    char pl_name[16];
    int pl_namelen;		/* Cuts back on strlen's */
    int pl_armies;
    int pl_info;		/* Teams which have info on planets */
    int pl_deadtime;		/* Time before planet will support life */
    int pl_couptime;		/* Time before coup may take place */

    /* new */

    int	pl_mydist;
    int	pl_justtaken;
    int	pl_needsbombed;
};

struct planetmatch {
   char			*pm_name;
   char			*pm_short;
   struct planet	*pm_planet;
};

#define MVALID 0x01
#define MINDIV 0x02
#define MTEAM  0x04
#define MALL   0x08
#define MDISTR 0xC0

struct message {
    int m_no;
    int m_flags;
    int m_time;
    int m_recpt;
    char m_data[80];
};

/* message control structure */

struct mctl {
    int mc_current;
};

/* This is a structure used for objects returned by mouse pointing */

#define PLANETTYPE 0x1
#define PLAYERTYPE 0x2

struct obtype {
    int o_type;
    int o_num;
};

struct rank {
    float hours, ratings, defense;
    char *name;
};

struct memory {
    struct player	players[MAXPLAYER];
    struct torp		torps[MAXPLAYER * MAXTORP];
    struct plasmatorp   plasmatorps[MAXPLAYER * MAXPLASMA];
    struct status	status[1];
    struct planet	planets[MAXPLANETS];
    struct phaser	phasers[MAXPLAYER];
    struct mctl		mctl[1];
    struct message	messages[MAXMESSAGE];
};

enum dist_type {
        /* help me do series */
        TAKE=1, OGG, BOMB, SPACE_CONTROL, HELP1, HELP2, HELP3, HELP4,

        /* doing series */
        ESCORTING, OGGING, BOMBING, CONTROLLING, DOING1, DOING2, DOING3, DOING4,

        /* other info series */
        FREE_BEER, /* ie. player x is totally hosed now */
        NO_GAS, /* ie. player x has no gas */
        CRIPPLED, /* ie. player x is way hurt but may have gas */
        PICKUP, /* player x picked up armies */
        POP, /* there was a pop somewhere */
        CARRYING, /* I am carrying */
        OTHER1, OTHER2,

        /* just a generic distress call */
        GENERIC };

        /* help1-4, doing1-4, and other1-2 are for future expansion */

/* from BRM struct.h */
struct distress {
    unsigned char sender;
    unsigned char dam, shld, arms, wtmp, etmp, fuelp, sts;
    unsigned char wtmpflag, etempflag, cloakflag, distype, macroflag, ttype;
    unsigned char close_pl, close_en, target, tclose_pl, tclose_en, pre_app, i;
    unsigned char close_j, close_fr, tclose_j, tclose_fr;
    unsigned char cclist[6]; /* allow us some day to cc a message 
                                up to 5 people */
        /* sending this to the server allows the server to do the cc action */
        /* otherwise it would have to be the client ... less BW this way */
    char preappend[80]; /* text which we pre or append */
};

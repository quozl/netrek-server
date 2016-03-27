/*
 * defs.h
 */
#include "copyright.h"

/* fast hypot test */
#define	hypot	ihypot

#define MAXPLAYER 36
#define TESTERS 4	/* Priveledged slots for robots and game 'testers' */
#define MAXPLANETS 40
#define MAXTORP 8
#define MAXPLASMA 1
#define PKEY 101
#define WINSIDE 500     /* Size of strategic and tactical windows */
#define BORDER 4        /* border width for option windows */
#define PSEUDOSIZE 16
#define CLOAK_PHASES 12  /* number of drawing phases in a cloak engage/disengage */
#define NUMRANKS 9

#define SHIPDAMDIST 3000 /* At this range, an exploding ship do damage */
#define PLASDAMDIST	2500

/* These are configuration definitions */

#define GWIDTH 100000   /* galaxy is 100000 spaces on a side */
#define WARP1 20	/* warp one will move 20 spaces per update */
#define SCALE 40	/* Window will be one pixel for 20 spaces */
#define EXPDIST 350	/* At this range a torp will explode */
#define DAMDIST 2000	/* At this range a torp does damage */
#define DETDIST 1800	/* At this range a player can detonate a torp */
#define PHASEDIST 6000	/* At this range a player can do damage with phasers */
#define ENTORBDIST 900	/* At this range a player can orbit a planet */
#define ORBDIST 800	/* A player will orbit at this radius */
#define ORBSPEED 2	/* This is the fastest a person can go into orbit */
#define PFIREDIST 1500	/* At this range a planet will shoot at a player */
#define UPDATE 100000	/* Update time is 100000 micro-seconds */
#define AUTOQUIT 60	/* auto logout in 60 secs */
#define VACANT -1       /* indicates vacant port on a starbase */
#define DOCKDIST 600
#define DOCKSPEED 2     /* If base is moving, there will be some
			   finesse involved to dock */
#define SBFUELMIN 10000   /* If starbase's fuel is less than this, it will not
			   refuel docked vessels */
#define TRACTDIST   6000 /* maximum effective tractor beam range */
#define TRACTEHEAT  5    /* ammount tractor beams heat engines */
#define TRACTCOST   20	 /* fuel cost of activated tractor beam */

/* These are memory sections */
#define PLAYER 1
#define MAXMESSAGE 50
#define MAXREVIEWMESSAGE 20

#define MSG_LEN		80

#define rosette(x)   ((((x) + 8) / 16) & 15)
/* #define rosette(x)   ((((x) + 256/VIEWS/2) / (256/VIEWS) + VIEWS) % VIEWS) */
/*                      (((x + 8) / 16 + 16)  %  16)  */

/* These are the teams */
/* Note that I used bit types for these mostly for messages and
   war status.  This was probably a mistake.  It meant that Ed
   had to add the 'remap' area to map these (which are used throughout
   the code as the proper team variable) into a nice four team deep
   array for his color stuff.  Oh well.
*/
#define NOBODY 0x0
#define FED 0x1
#define ROM 0x2
#define KLI 0x4
#define ORI 0x8

#define FED_N	0
#define ROM_N	1
#define KLI_N	2
#define ORI_N	3

#define ALLTEAM (FED|ROM|KLI|ORI)
#define MAXTEAM (ORI)
#define NUMTEAM 4
/*
** These are random configuration variables
*/
#define VICTORY 3	/* Number of systems needed to conquer the galaxy */
#define WARNTIME 30	/* Number of updates to have a warning on the screen */
#define MESSTIME 30	/* Number of updates to have a message on the screen */

#define TARG_PLAYER	0x1	/* Flags for gettarget */
#define TARG_PLANET	0x2
#define TARG_CLOAK	0x4	/* Include cloaked ships in search */
#define TARG_SELF	0x8


/* Other stuff that Ed added */

#define ABS(a)			/* abs(a) */ (((a) < 0) ? -(a) : (a))
#define MAX(a,b)		((a) > (b) ? (a) : (b))

#define myPlasmaTorp(t)		(me->p_no == (t)->pt_owner)
#define myTorp(t)		(me->p_no == (t)->t_owner)
#define friendlyPlasmaTorp(t)	((!(me->p_team & (t)->pt_war)) || (myPlasmaTorp(t)))
#define friendlyTorp(t)		((!(me->p_team & (t)->t_war)) || (myTorp(t)))
#define myPhaser(p)		(&phasers[me->p_no] == (p))
#define friendlyPhaser(p)	(me->p_team == players[(p) - phasers].p_team)
#define myPlayer(p)		(me == (p))
#define myPlanet(p)		(me->p_team == (p)->pl_owner)
#define friendlyPlayer(p)	((!(me->p_team & \
				    ((p)->p_swar | (p)->p_hostile))) && \
				    (!((p)->p_team & \
				    (me->p_swar | me->p_hostile))))
#define isAlive(p)		((p)->p_status == PALIVE)
#define friendlyPlanet(p)	((p)->pl_info & me->p_team && \
			     !((p)->pl_owner & (me->p_swar | me->p_hostile)))

#define torpColor(t)		\
	(myTorp(t) ? myColor : shipCol[remap[players[(t)->t_owner].p_team]])
#define plasmatorpColor(t)		\
	(myPlasmaTorp(t) ? myColor : shipCol[remap[players[(t)->pt_owner].p_team]])
#define phaserColor(p)		\
	(myPhaser(p) ? myColor : shipCol[remap[players[(p) - phasers].p_team]])
/* 
 * Cloaking phase (and not the cloaking flag) is the factor in determining 
 * the color of the ship.  Color 0 is white (same as 'myColor' used to be).
 */
#define playerColor(p)		\
	(myPlayer(p) ? myColor : shipCol[remap[(p)->p_team]])
#define planetColor(p)		\
	(((p)->pl_info & me->p_team) ? shipCol[remap[(p)->pl_owner]] : unColor)

#define bombingRating(p)	\
	((float) (p)->p_stats.st_tarmsbomb * status->timeprod / \
	 ((float) (p)->p_stats.st_tticks * status->armsbomb))
#define planetRating(p)		\
	((float) (p)->p_stats.st_tplanets * status->timeprod / \
	 ((float) (p)->p_stats.st_tticks * status->planets))
#define offenseRating(p)	\
	((float) (p)->p_stats.st_tkills * status->timeprod / \
	 ((float) (p)->p_stats.st_tticks * status->kills))
#define defenseRating(p)	\
	((float) (p)->p_stats.st_tticks * status->losses / \
	 ((p)->p_stats.st_tlosses!=0 ? \
	  ((float) (p)->p_stats.st_tlosses * status->timeprod) : \
	  (status->timeprod)))

typedef enum {FALSE=0, TRUE} boolean;

#define sendTorpReq(dir) sendShortPacket(CP_TORP, dir)
#define sendPhaserReq(dir) sendShortPacket(CP_PHASER, dir)
#define sendSpeedReq(speed) sendShortPacket(CP_SPEED, speed)
#define sendDirReq(dir) sendShortPacket(CP_DIRECTION, dir)
#define sendShieldReq(state) sendShortPacket(CP_SHIELD, state)
#define sendOrbitReq(state) sendShortPacket(CP_ORBIT, state)
#define sendRepairReq(state) sendShortPacket(CP_REPAIR, state)
#define sendBeamReq(state) sendShortPacket(CP_BEAM, state)
#define sendCopilotReq(state) sendShortPacket(CP_COPILOT, state)
#define sendDetonateReq() sendShortPacket(CP_DET_TORPS, 0)
#define sendCloakReq(state) sendShortPacket(CP_CLOAK, state)
#define sendBombReq(state) sendShortPacket(CP_BOMB, state)
#define sendPractrReq() sendShortPacket(CP_PRACTR, 0)
#define sendWarReq(mask) sendShortPacket(CP_WAR, mask)
#define sendRefitReq(ship) sendShortPacket(CP_REFIT, ship)
#define sendPlasmaReq(dir) sendShortPacket(CP_PLASMA, dir)
#define sendPlaylockReq(pnum) sendShortPacket(CP_PLAYLOCK, pnum)
#define sendPlanlockReq(pnum) sendShortPacket(CP_PLANLOCK, pnum)
#define sendCoupReq() sendShortPacket(CP_COUP, 0)
#define sendQuitReq() sendShortPacket(CP_QUIT, 0)
#define sendByeReq() sendShortPacket(CP_BYE, 0)
#define sendDockingReq(state) sendShortPacket(CP_DOCKPERM, state)
#define sendResetStatsReq(verify) sendShortPacket(CP_RESETSTATS, verify)
#ifdef SCANNERS
#define sendScanReq(who) sendShortPacket(CP_SCAN, who)          /* ATM */
#endif

#define sendShortReq(state) sendShortPacket(CP_S_REQ, state)

/* This macro allows us to time things based upon # frames / sec.
 */
#define ticks(x) ((x)*200000/timerDelay)

char *getdefault();

#ifdef ATM
/*
 * UDP control stuff
 */
#ifdef GATEWAY
# define UDP_NUMOPTS    11
# define UDP_GW         UDP_NUMOPTS-1
#else
# define UDP_NUMOPTS    10
#endif
#define UDP_CURRENT     0
#define UDP_STATUS      1
#define UDP_DROPPED     2
#define UDP_SEQUENCE    3
#define UDP_SEND        4
#define UDP_RECV        5
#define UDP_DEBUG       6
#define UDP_FORCE_RESET 7
#define UDP_UPDATE_ALL	8
#define UDP_DONE        9
#define COMM_TCP        0
#define COMM_UDP        1
#define COMM_VERIFY     2
#define COMM_UPDATE	3
#define COMM_MODE       4
#define SWITCH_TCP_OK   0
#define SWITCH_UDP_OK   1
#define SWITCH_DENIED   2
#define SWITCH_VERIFY   3
#define CONNMODE_PORT   0
#define CONNMODE_PACKET 1
#define STAT_CONNECTED  0
#define STAT_SWITCH_UDP 1
#define STAT_SWITCH_TCP 2
#define STAT_VERIFY_UDP 3
#define MODE_TCP        0
#define MODE_SIMPLE     1
#define MODE_FAT	2
#define MODE_DOUBLE     3

#define UDP_RECENT_INTR 300
#define UDP_UPDATE_WAIT	5

/* client version of UDPDIAG */
#define UDPDIAG(x)      { if (udpDebug) { printf("UDP: "); printf x; }}
#define V_UDPDIAG(x)    /*UDPDIAG(x)*/
#endif /* ATM */

#define RANDOM()	rand()
#define SRANDOM(x)	srand(x)

/* compiler defines to tweak robot behavior */
#define NO_PFORBIT 1 /* activate some code */

/* define this if you do not wish the bots to respond to commands */
#undef	BOTS_IGNORE_COMMANDS

/* redefine this to your local robot password */
#if 0
#define ROBOTPASS "CHANGEME"
#endif

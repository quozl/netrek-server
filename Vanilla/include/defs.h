/* $Id: defs.h,v 1.7 2006/04/26 09:52:43 quozl Exp $
 */

#ifndef _h_defs
#define _h_defs

#include "copyright.h"
#include "config.h"

#ifndef NULL
#define NULL	((char *) 0)
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* some OS's don't define SA_NOCLDWAIT */
#ifndef SA_NOCLDWAIT
#define SA_NOCLDWAIT	0
#endif

#define ERROR(l,x)	{ if (errorlevel >= l) { printf x; } }

#define FNAMESIZE	256

#define MAXHOSTNAMESIZE	64	/* maximum host name size in characters	*/
#define MAXBANS 32

#define NODET (char)(-1) /* isae - chain reaction case */

#define RESERVED_SIZE	16 /* critical to protocol */

#ifdef RSA
#define RSA_VERSION	"RSA v2.0 SERVER"
#define KEY_SIZE	32
#endif
#define MSG_LEN		80 /* Length of message (critical to protocol) */
#define MSGPREFIX_LEN	10 /* Length of "xx->xx" text including spaces */
#define MSGTEXT_LEN	(MSG_LEN - MSGPREFIX_LEN - 1) /* Maximum text length minus prefix and \0 */
#define NAME_LEN	16 /* critical to protocol */
#define KEYMAP_LEN	96 /* critical to protocol */
#define ADMIN_PASS_LEN	(MSG_LEN - 20)

#ifdef MATH_ERR_CHECK
#   define MATH_ERR_CHECK(v)            \
    {                                    \
	extern int errno;                 \
	if(errno == 33){ /* EDOM */       \
	    char      buf[80];             \
	    sprintf(buf, "%s:%d: %d", __FILE__, __LINE__, v);      \
	    perror(buf);                   \
	}                                 \
    }
#else
#   define MATH_ERR_CHECK(v)    /* nothing */
#endif


#define RECHECK_CLUE	0	/* recheck at CLUEINTERVAL if set */
#define CLUEINTERVAL	9000	/* 30 minute timer */
#define ONEMINUTE       300
#define THREEMINUTES    900
#define FIVEMINUTES     1500
#define TENMINUTES      3000
#define FIFTEENMINUTES  4500

#define DOOSHMSG	254	/* NBT for doosh messages */

#define UNDEF		-1	/* for new_warning()	*/

#define CVOID 		struct player_spacket *

#define MAXPLAYER 32
#define TESTERS (MAXPLAYER - 16)   /* Reserved slots for observers and robots,
                                   by default have 16 player slots and make
                                   the rest obs slots */

#define PV_EXTRA  8			/* # of slot-independent vote types */
#define PV_TOTAL  MAXPLAYER*3+PV_EXTRA	/* total number of voting slots     */
#define PV_EJECT  0*MAXPLAYER		/* array base for eject votes       */
#define PV_BAN    1*MAXPLAYER		/* array base for ban votes         */
#define PV_NOPICK 2*MAXPLAYER		/* array base for nopick votes      */
#define PV_OTHER  3*MAXPLAYER		/* array base for non-eject votes   */
/* see ntserv/ntscmds.c for voting array */

#if defined(NEWBIESERVER) || defined(PRETSERVER)
#define MAXQUEUE 14	/* Number of different waitqueues */
#else
#define MAXQUEUE 9
#endif
#define MAXWAITING 32   /* Number of total people waiting */
#define QNAMESIZE  20   /* Max length of wait queue name */
#define MAXPLANETS 40
#define MAXTORP 8
#define MAXPLASMA 1
#ifndef PKEY
#define PKEY 128
#endif
#define WINSIDE 500     /* Size of strategic and tactical windows */
#define BORDER 4        /* border width for option windows */
#define PSEUDOSIZE 16
#define CLOAK_PHASES 12  /* number of drawing phases in a cloak engage/disengage */
#define NUMRANKS 9
#define RANK_ADMIRAL 8  /* Max rank used by clients in absence of SP_RANK */

#define SURREND 4

#ifdef SURRENDER_SHORT /* Length of surrender timer in minutes */
#define SURRLENGTH 20  /* Short timer */
#else
#define SURRLENGTH 30  /* Long Timer */
#endif 

#define NUM_TYPES 8
#define SCOUT 0
#define DESTROYER 1
#define CRUISER 2
#define BATTLESHIP 3
#define ASSAULT 4
#define STARBASE 5
#define SGALAXY 6
#define ATT 7

/* These are configuration definitions */

#define GWIDTH 100000   /* galaxy is 100000 spaces on a side */
#define WARP1 20	/* warp one will move 20 spaces per update */
#define SCALE 40	/* Window will be one pixel for 20 spaces */
#define EXPDIST 350	/* At this range a torp will explode */
#define ZAPPLAYERDIST 390 /* Phaser will hit player if line is this close */
#define ZAPPLASMADIST 270 /* Phaser will hit plasma if line is this close */
#define DAMDIST 2000	/* At this range a torp does damage */
#define SHIPDAMDIST 3000 /* At this range, an exploding ship does damage */
#define PLASDAMDIST 2500 /* At this range, a plasma does damage */
#define DETDIST 1700	/* At this range a torp can be detonated */
#define PHASEDIST 6000	/* At this range a player can do damage with phasers */
#define ENTORBDIST 900	/* At this range a player can orbit a planet */
#define ORBDIST 800	/* A player will orbit at this radius */
#define ORBSPEED 2	/* This is the fastest a person can go into orbit */
#define PFIREDIST 1500	/* At this range a planet will shoot at a player */
#ifdef undef
/* it is planned that the server will eventually be operable in a slower or
faster reality mode, but we need to fix some robots before adopting this,
because they have initialisation of variables dependent on UPDATE. */
#define UPDATE reality
#else
#define UPDATE 100000	/* Update time is 100000 micro-seconds */
#endif
#define TPF (fps/10)	/* ticks per frame */
#define SPM 256		/* scaled position multiplier */
#define SPB 8 		/* scaled position bits */
#define spi(x) ((x) << SPB)	/* convert in to scaled position */
#define spo(x) ((x) >> SPB)	/* convert out from scaled position */
#define AUTOQUIT 60	/* auto logout in 60 secs */
#define VACANT -1       /* indicates vacant port on a starbase */
#define DOCKDIST 600
#define DOCKSPEED 2     /* If base is moving, there will be some
			   finesse involved to dock */
#define NUMBAYS 4	/* number of docking bays a starbase has */
#define SBFUELMIN 10000   /* If starbase's fuel is less than this, it will not
			   refuel docked vessels */
#define TRACTDIST   6000 /* maximum effective tractor beam range */
#define TRACTEHEAT  5    /* ammount tractor beams heat engines */
#define TRACTCOST   20	 /* fuel cost of activated tractor beam */

/* These are memory sections */
#define PLAYER 1
#define MAXMESSAGE 75
#define MAXUPLINK 64
#define MAXREVIEWMESSAGE 20

#define timeprod_int()		(int)(status->timeprod/10.0)

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
#define ALLTEAM (FED|ROM|KLI|ORI)
#define MAXTEAM (ALLTEAM)	/* was ALLTEAM (overkill?) 6/22/92 TMC */
				/* no, not overkill. ALLTEAM used to be
				   larger than MAXTEAM which overran bounds
				   in team[], nbt */

#define NUMTEAM 4
/*
** These are random configuration variables
*/

#ifndef TWO_RACE
#define VICTORY 3       /* Number of systems needed to conquer the galaxy */
#else
#define VICTORY 2       /* 2-Race Genocide, isae */
#endif

#define WARNTIME 30	/* Number of updates to have a warning on the screen */
#define MESSTIME 30	/* Number of updates to have a message on the screen */

#define MIN_COUP_TIME   30	/* Min/max minutes before a geno'ed race */
#define MAX_COUP_TIME   60	/* can reenter and coup (random) 4/15/92 TC */

#define TARG_PLAYER	0x1	/* Flags for gettarget */
#define TARG_PLANET	0x2
#define TARG_CLOAK	0x4	/* Include cloaked ships in search */
#define TARG_SELF	0x8

/* Environment variable name. added 11/6/92 DRG */
#define ENV_NAME        "NTSERV_PATH"

/* Data files to make the game play across daemon restarts. */

#define N_GLOBAL	"global"
#define N_SCORES	"scores"
#define N_PLFILE	"planets"
#define N_MOTD		"motd"
#define N_MOTD_CLUE	"motd_clue"
#define N_DAEMON	"daemon"
#define N_ROBOT		"robotII"
#define N_LOGFILENAME	"logfile"
#define N_PLAYERFILE	"players"
#define N_PLAYERINDEXFILE	"players.index"
#define N_CONQFILE	"conquer"
#define N_SYSDEF_FILE	"sysdef"
#define N_TIME_FILE	"time"
#define N_CLUE_BYPASS	"clue-bypass"
#define N_BANNED_FILE	"banned"
#define N_SCUM_FILE	"god.LOG"
#define N_ERROR_FILE	"ERRORS"
#define N_BYPASS_FILE	"bypass"
#ifdef RSA
#define N_RSA_KEY_FILE	"reserved"
#endif
#ifdef AUTOMOTD
#define N_MAKEMOTD	"/makemotd"
#endif
#define N_MESGLOG	"mesg.LOG"
#define N_GODLOG	"god.LOG"
#define N_FEATURE_FILE	"features"

#define N_NOCOUNT_FILE	"nocount"
#define N_PROG		"ntserv"
#define N_LOGFILE	"log"

#define NO_ROBOT	0

#ifdef BASEPRACTICE
#define N_BASEP		"basep"
#define BASEP_ROBOT	1
#define N_MOTD_BASEP	"motd_basep"
#endif

#ifdef NEWBIESERVER
#define N_NEWBIE	"newbie"
#define NEWBIE_ROBOT	5
#endif

#ifdef PRETSERVER
#define N_PRET		"pret"
#define PRET_ROBOT	6
#endif

#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
#define N_ROBODIR       "og"
#endif

#ifdef DOGFIGHT
#define N_MARS		"mars"
#define MARS_ROBOT	2
#endif

#define N_PUCK		"puck"
#define PUCK_ROBOT	3

#define N_INL		"inl"
#define INL_ROBOT	4

#define N_CAMBOT	"cambot"
#define N_CAMBOT_OUT	"cambot.pkt"

#define N_NETREKDPID    LOCALSTATEDIR"/netrekd.pid"
#define N_PORTS         SYSCONFDIR"/ports"

/* Other stuff that Ed added */

#define ABS(a)			/* abs(a) */ (((a) < 0) ? -(a) : (a))
#ifndef MAX
#define MAX(a,b)		((a) > (b) ? (a) : (b))
#endif


#define planet_(i)       (planets + (i))
/* Note that plasmas and torps are in the same array, plasmas after torps. */
#define plasma_(i)       (torp_(MAXTORP*MAXPLAYER + (i)))
#define player_(i)       (players + (i))
#define torp_(i)         (torps + (i))
                       
#define firstPlayer      (player_(0))
#define lastPlayer       (player_(MAXPLAYER - 1))
                       
#define firstPlanet      (planet_(0))
#define lastPlanet       (planet_(MAXPLANETS - 1))
                       
#define firstTorp        (torp_(0))
#define lastTorp         (torp_(MAXTORP*MAXPLAYER - 1))
                       
#define firstTorpOf(p)   (torp_(MAXTORP*(p)->p_no))
#define lastTorpOf(p)    (torp_(MAXTORP*(p)->p_no + MAXTORP - 1))
                       
#define firstPlasma      (plasma_(0))
#define lastPlasma       (plasma_(MAXPLASMA*MAXPLAYER - 1))

#define firstPlasmaOf(p) (plasma_(MAXPLASMA*(p)->p_no))
#define lastPlasmaOf(p)  (plasma_(MAXPLASMA*(p)->p_no + MAXPLASMA - 1))

                                   
#define badPlayerNo(p)          ((p) < 0 || (p) > (MAXPLAYER-1))

#define enemyPlasmaTorp(t)	enemyTorp(t)
#define enemyTorp(t)		((me->p_team & (t)->t_war) || \
				 ((t)->t_team & me->p_war))
#define friendlyPlasmaTorp(t)	friendlyTorp(t)
#define friendlyTorp(t)		(! enemyTorp(t))
#define myPlasmaTorp(t)		myTorp(t)
#define myTorp(t)               (me->p_no == (t)->t_owner)

#define friendlyPhaser(p)	(me->p_team == players[(p) - phasers].p_team)
#define myPlayer(p)		(me == (p))
#define myPlanet(p)		(me->p_team == (p)->pl_owner)
#define friendlyPlayer(p)	((!(me->p_team & (p)->p_swar)) && \
				 (!((p)->p_team & me->p_war)))
#define isAlive(p)		((p)->p_status == PALIVE)
#define friendlyPlanet(p)	(((p)->pl_info & me->p_team) && \
			         (!((p)->pl_owner & me->p_war)))

#define plasmatorpColor(t)	torpColor(t)
#define torpColor(t)		\
	(myTorp(t) ? myColor : shipCol[remap[players[(t)->t_owner].p_team]])
#define phaserColor(p)		\
	(myPhaser(p) ? myColor : shipCol[remap[players[(p) - phasers].p_team]])
/* 
 * Cloaking phase (and not the cloaking flag) is the factor in determining 
 * the color of the ship.  Color 0 is white (same as 'myColor' used to be).
 */
#define playerColor(p)		\
	(myPlayer(p) ? 		\
	    (cloak_pixels[0][me->p_cloakphase])	\
	    : (cloak_pixels[remap[(p)->p_team]][(p)->p_cloakphase]))
#define planetColor(p)		\
	(((p)->pl_info & me->p_team) ? shipCol[remap[(p)->pl_owner]] : unColor)

#define planetFont(p)		\
	(myPlanet(p) ? bfont : friendlyPlanet(p) ? ifont : dfont)
#define shipFont(p)		\
	(myPlayer(p) ? bfont : friendlyPlayer(p) ? ifont : dfont)

#ifndef LTD_STATS

#define bombingRating(p)	\
	((float) (p)->p_stats.st_tarmsbomb * status->timeprod / \
	 ((float) (p)->p_stats.st_tticks * (status->armsbomb?status->armsbomb:1)))
#define planetRating(p)		\
	((float) (p)->p_stats.st_tplanets * status->timeprod / \
	 ((float) (p)->p_stats.st_tticks * status->planets))
#define offenseRating(p)	\
	((float) (p)->p_stats.st_tkills * status->timeprod / \
	 ((float) (p)->p_stats.st_tticks * (status->kills?status->kills:1)))
#define defenseRating(p)	\
	((float) (p)->p_stats.st_tticks * status->losses / \
	 ((p)->p_stats.st_tlosses!=0 ? \
	  ((float) (p)->p_stats.st_tlosses * status->timeprod) : \
	  (status->timeprod)))

#endif /* LTD_STATS */

/*
 * Time things based upon the SIGALRM signal.
 * Given a number of one fifth seconds, it will return the number of
 * SIGALRMs we will receive in that period.
 */
#define efticks(x) (((x)*(fps/5))/me->p_fpu)

/*
 * UDP control stuff
 */
#ifdef GATEWAY
# define UDP_NUMOPTS	11
# define UDP_GW		UDP_NUMOPTS-1
#else
# define UDP_NUMOPTS	10
#endif
#define UDP_CURRENT	0
#define UDP_STATUS	1
#define UDP_DROPPED	2
#define UDP_SEQUENCE	3
#define UDP_SEND	4
#define UDP_RECV	5
#define UDP_DEBUG	6
#define UDP_FORCE_RESET	7
#define UDP_UPDATE_ALL	8
#define UDP_DONE	9
#define COMM_TCP	0
#define COMM_UDP	1
#define COMM_VERIFY	2
#define COMM_UPDATE	3
#define COMM_MODE	4       /* put this one last */
#define SWITCH_TCP_OK	0
#define SWITCH_UDP_OK	1
#define SWITCH_DENIED	2
#define SWITCH_VERIFY	3
#define CONNMODE_PORT	0
#define CONNMODE_PACKET	1
#define STAT_CONNECTED	0
#define STAT_SWITCH_UDP	1
#define STAT_SWITCH_TCP	2
#define STAT_VERIFY_UDP	3
#define MODE_TCP	0
#define MODE_SIMPLE	1
#define MODE_FAT	2
#define MODE_DOUBLE	3	/* put this one last */

#define UDP_RECENT_INTR	300
#define UDP_UPDATE_WAIT 5	/* 5 second wait */

/* server version of UDPDIAG */
/* (change these to "#define UDPDIAG(x) <return>" for smaller & faster code) */
#define UDPDIAG(x)	{ if (udpAllowed > 1) { printf("%s: UDP: ", me->p_mapchars); printf x; }}
#define V_UDPDIAG(x)	{ if (udpAllowed > 2) { printf("%s: UDP: ", me->p_mapchars); printf x; }}


#if WINSIDE > 500
    SHORT_PACKETS_DOES_NOT_WORK_FOR_WINSIDE_GREATHER_THAN_500
#endif
#define TEXTE 0
#define PHASER_HIT_TEXT 1
#define BOMB_INEFFECTIVE 2
#define BOMB_TEXT 3
#define BEAMUP_TEXT 4
#define BEAMUP2_TEXT 5
#define BEAMUPSTARBASE_TEXT 6
#define BEAMDOWNSTARBASE_TEXT 7
#define BEAMDOWNPLANET_TEXT 8
#define SBREPORT 9
#define ONEARG_TEXT 10
#define BEAM_D_PLANET_TEXT 11
#define ARGUMENTS 12
#define BEAM_U_TEXT 13
#define LOCKPLANET_TEXT 14
#define LOCKPLAYER_TEXT 15
#define SBRANK_TEXT 16
#define SBDOCKREFUSE_TEXT 17
#define SBDOCKDENIED_TEXT 18
#define SBDOCKREFUSE_TEXT 17
#define SBDOCKDENIED_TEXT 18
#define SBLOCKSTRANGER 19
#define SBLOCKMYTEAM 20
/*      Daemon messages */
#define DMKILL 21
#define KILLARGS 22
#define DMKILLP 23
#define DMBOMB 24
#define DMDEST 25
#define DMTAKE 26
#define DGHOSTKILL 27
/*      INL     messages                */
#define INLDMKILLP 28
#define INLDMKILL 29    /* Because of shiptypes */
#define INLDRESUME 30
#define INLDTEXTE 31
/* Variable warning stuff */
#define STEXTE 32               /* static text that the server needs to send to the client first */
#define SHORT_WARNING 33 /* like CP_S_MESSAGE */
#define STEXTE_STRING 34
#define KILLARGS2 35		/* For Why dead messages */

/* Warning flags , used in socket.c */
#define SVALID   253 /* Mark server messages as valid to send over SP_S_MESSAGE */
#define SINVALID 254    /* Mark server Message as invalid to send over SP_S_MESSAGE */
#define DINVALID 255    /* Mark daemon ( GOD) Message as invalid to send over SP_S_WARNING */

/* #undef PUCK_FIRST  99 */  /* make sure puck runs before players do */

/* for t-mode messaging, change this according to political environment */
#define WARMONGER "George W Bush"

/* openmem, lock functions */
#define MAXLOCKS       32       /* number of locks to be allocated        */
#define LOCK_PICKSLOT   0       /* prevent PFREE to POUTFIT race          */
#define LOCK_SETGAME    1       /* general purpose setgame tool lock      */
#define LOCK_PLAYER_ADD 2       /* player database growth file lock       */
#define LOCK_QUEUE_ADD  3       /* adding an incoming connection to queue */

#define NLOCKS          3       /* number of locks with a defined purpose */

#define IGNORING	1	/* next 2 are for state checks in the     */
#define IGNOREDBY	2	/* do_display_ignores func. in ntscmds.c  */

/* SP_BADVERSION reason codes */
#define BADVERSION_SOCKET   0 /* CP_SOCKET version does not match, exiting */
#define BADVERSION_DENIED   1 /* access denied by netrekd */
#define BADVERSION_NOSLOT   2 /* no slot on queue */
#define BADVERSION_BANNED   3 /* banned */
#define BADVERSION_DOWN     4 /* game shutdown by server */
#define BADVERSION_SILENCE  5 /* daemon stalled */
#define BADVERSION_SELECT   6 /* internal error */
/* as at 2008-06-21 netrek-client-cow and netrek-client-xp report 0 as
   invalid version, 1-6 as cannot play, others are not valid, and
   netrek-client-xp uses a popup message box for 1-6 */

#endif /* _h_defs */

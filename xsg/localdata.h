/*
 * data.h
 */
#include "copyright.h"
#include "config.h"
#include <string.h>

#ifdef NBR
#include "data.h"
#else

#define cloud_width 		9
#define cloud_height 		9
#define plasmacloud_width 	13
#define plasmacloud_height 	13
extern struct player *me;
extern int oldalert;
extern int remap[];
extern int nopilot;
extern int mapmode; 
extern int namemode; 
extern int showShields;
extern int warncount;
extern int warntimer;
extern int infomapped;
extern int scanmapped;	/* ATM - scanner stuff */
extern int mustexit;
extern int messtime;
extern int keeppeace;
extern int showlocal, showgalactic;
extern char *shipnos;
extern int sock;
extern int xtrekPort;
extern int queuePos;
extern int pickOk;
extern int lastRank;
extern int promoted;
extern int loginAccept;
extern int tournMask;
extern char *serverName;
extern int loggedIn;
extern int chaos;
extern int udpAllowed;
extern char teamlet[];
extern char *teamshort[];
extern struct rank ranks[NUMRANKS];
extern int tournplayers;
extern int plkills;
extern int hourratio;
extern int hiddenenemy;
extern float maxload;
extern int cyborg;		/* ATM */
extern int testtime;
#endif

extern int reportKills;
extern int msgBeep;	/* ATM - msg beep */
extern int oldmctl;
extern W_Window robotwin, optionWin, modifyWin;
extern unsigned localflags;
extern int playMode;
extern int plmode;
extern int playback;
extern int updateSpeed;
extern int extracting;	/* 7/27/93 BM */
extern int restarting;		/* cameron@stl.dec.com 30-Apr-1997 */
extern int reinitPlanets;
extern int show_xsg_posn;
extern int mapfire;		/* TSH 2/10/93 */
extern int runclock;		/* TSH 2/93 */
extern int frameskip;		/* TSH 4/93 */
extern int record;
extern char *recfile;
extern int whichFrame;
extern int latestFrame;
extern int allocFrames;
extern int currentFrame;	/* 7/30/93 BM */
extern int recordSpeed;
extern int lastUpdate[];

#define EX_FRAMES 		5
#define SBEXPVIEWS 		7
#define NUMDETFRAMES		5	/* # frames in torp explosion */
#define ex_width        	64
#define ex_height       	64
#define sbexp_width        	80
#define sbexp_height       	80
#define etorp_width 		3
#define etorp_height 		3
#define eplasmatorp_width 	7
#define eplasmatorp_height 	7
#define mplasmatorp_width 	5
#define mplasmatorp_height 	5
#define mtorp_width 		3
#define mtorp_height 		3
#define crossmask_width 	16
#define crossmask_height 	16
#define planet_width 		30
#define planet_height 		30
#define mplanet_width 		16
#define mplanet_height 		16
#define shield_width 		20
#define shield_height 		20
#define cloak_width		20
#define cloak_height		20
#define icon_width 		112
#define icon_height 		80

extern struct player *players;
extern struct torp *torps;
extern struct plasmatorp *plasmatorps;
extern struct status *status;
extern struct ship *myship;
extern struct stats *mystats;
extern struct planet *planets;
extern struct phaser *phasers;
extern struct message *messages;
extern struct mctl *mctl;
extern struct team *teams;
extern struct memory *sharedMemory;    /* for game recorder */
extern struct memory universe;
extern struct planet pdata[];

extern int udcounter;
extern int messpend;
extern int lastcount;
extern int mdisplayed;
extern int redrawall;
extern int watch;
extern int selfdest;
extern int lastm;
extern int delay;
extern int rdelay;
extern int showStats;

extern int nextSocket;
extern int updatePlayer[];
extern int updatePlanet[];
extern int redrawPlayer[];
extern int uspeeds[];		/* table of update speeds in option.c */

extern double	Sin[], Cos[];

extern W_Icon stipple, clockpic, icon;

#define VIEWS 16

extern W_Icon expview[EX_FRAMES];
extern W_Icon sbexpview[SBEXPVIEWS];
extern W_Icon cloud[NUMDETFRAMES];
extern W_Icon plasmacloud[NUMDETFRAMES];
extern W_Icon etorp, mtorp;
extern W_Icon eplasmatorp, mplasmatorp;
extern W_Icon shield, cloakicon, xsgbits;
extern W_Icon fed_bitmaps[NUM_TYPES][VIEWS],
       	      kli_bitmaps[NUM_TYPES][VIEWS],
       	      rom_bitmaps[NUM_TYPES][VIEWS],
       	      ori_bitmaps[NUM_TYPES][VIEWS],
	      ind_bitmaps[NUM_TYPES][VIEWS];
extern W_Icon bplanets[6];
extern W_Icon mbplanets[6];
extern W_Icon bplanets2[8];
extern W_Icon mbplanets2[8];

extern W_Color	borderColor, backColor, textColor, myColor, 
		warningColor, shipCol[5], rColor, yColor,
		gColor, unColor, foreColor;

extern char pseudo[PSEUDOSIZE];
extern char login[PSEUDOSIZE];


extern W_Window	messagew, w, mapw, statwin, baseWin, infow, iconWin, war,
		warnw, helpWin, plstatw, reviewWin;

extern int shipsallowed[];
extern int weaponsallowed[];
extern int startplanets[];
extern int binconfirm;
extern char godsname[];		/* TSH 2/93 */

#define WP_PLASMA 0
#define WP_TRACTOR 1
#ifdef HAS_SCANNER
# define WP_SCANNER 2		/* ATM */
# define WP_MAX 3
#else
# define WP_MAX 2
#endif


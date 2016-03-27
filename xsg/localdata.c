/*
 * data.c
 */
#include "copyright.h"

#include <stdio.h>
#include "Wlib.h"
#include "defs.h"
#include "xsg_defs.h"
#include "struct.h"
#include "localdata.h"

#ifndef NBR
struct player *me=NULL;
int	oldalert = PFGREEN;	/* Avoid changing more than we have to */
int 	remap[16] = { 0, 1, 2, 0, 3, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0 };
int	nopilot = 1;
int	mapmode = 2; 
int	namemode = 1; 
int	showShields = 1;
int	warncount = 0;
int	warntimer = -1;
int	infomapped = 0;
int	scanmapped = 0;		/* ATM - scanners */
int	mustexit = 0;
int	messtime = 5;
int	keeppeace = 0;
int	showlocal = 0;		/* ATM: show owner */
int 	showgalactic = 1;	/* ATM: show resources */
char 	*shipnos="0123456789abcdefghijklmnopqrstuvwxyz";
int 	sock= -1;
int	xtrekPort=6592;		/* ATM: was 592 (inetd version) */
int	queuePos= -1;
int	pickOk= -1;
int	lastRank= -1;
int	promoted= 0;
int	loginAccept= -1;
int	tournMask=15;
char   *serverName="crp11.gs.com";
int	loggedIn=0;
int	chaos = 0;
int	udpAllowed = 1;			/* UDP */
char teamlet[] = {'I', 'F', 'R', 'X', 'K', 'X', 'X', 'X', 'O'};
char *teamshort[9] = {"IND", "FED", "ROM", "X", "KLI", "X", "X", "X", "ORI"};
struct rank ranks[NUMRANKS] = {
    { 0.0, 0.0, 0.0, "Ensign"},
    { 2.0, 1.0, 0.0, "Lieutenant"},
    { 4.0, 2.0, 0.8, "Lt. Cmdr."}, 
    { 8.0, 3.0, 0.8, "Commander"},
    {15.0, 4.0, 0.8, "Captain"},
    {20.0, 5.0, 0.8, "Flt. Capt."},
    {25.0, 6.0, 0.8, "Commodore"},
    {30.0, 7.0, 0.8, "Rear Adm."},
    {40.0, 8.0, 0.8, "Admiral"}};

int tournplayers=5;
int plkills=2;
int binconfirm=1;
int hourratio=1;
int hiddenenemy=1;
float maxload=0.7;
int cyborg=0;		/* ATM */
int testtime;
#endif

int	reportKills=1;			/* report kill messages? */
int	msgBeep = 1;		/* ATM - msg beep */
int	oldmctl=0;			/* old message posn */
unsigned localflags=0;
int	playMode = FORWARD|DO_STEP;	/* "" */
int	plmode = 0;			/* planets/player stats */
int 	playback=0;
int	updateSpeed = 2;		/* default 2 updates/second */
int	extracting = 0;			/* 7/27/93 BM */
int	restarting = 0;			/* cameron@stl.dec.com 30-Apr-1997 */
char	*recfile = NULL;		/* game playback */
int 	reinitPlanets=0;
int	show_xsg_posn = 1;		/* show xsg "cursor" */
int mapfire=1;		/* TSH 2/10/93 */
int runclock=1;		/* TSH 2/93 */
int frameskip=20;	/* TSH 4/93 */
int 	record=0;
int     whichFrame = 0;
int     latestFrame = -1;
int     allocFrames = MAXFRAMES;
int	currentFrame = 0;		/* 7/30/93 BM */
int	recordSpeed = 2;		/* default 2 updates/second */
int	lastUpdate[MAXPLAYER]={0};	/* Last update of this player */

struct player *players;
struct torp *torps;
struct plasmatorp *plasmatorps;
struct status *status;
struct ship *myship;
struct stats *mystats;
struct planet *planets;
struct phaser *phasers;
struct message *messages;
struct mctl *mctl;
struct memory *sharedMemory;	/* for game recorder */
struct memory universe;		/* used to check for updates */

int	messpend;
int	lastcount;
int	mdisplayed;
int	redrawall;
int	selfdest;
int	udcounter;
int	lastm;
int	delay;			/* delay for decaring war */
int	rdelay;			/* delay for refitting */
int	showStats;

int 	nextSocket;	/* socket to use when we get ghostbusted... */
int	updatePlayer[MAXPLAYER];	/* Needs updating on player list */
int	updatePlanet[MAXPLANETS];	/* Needs updating on planet list */
int	redrawPlayer[MAXPLAYER];	/* Needs redrawing on galactic map */

W_Icon stipple, clockpic, icon;

W_Color	borderColor, backColor, textColor, myColor, warningColor, shipCol[5],
	rColor, yColor, gColor, unColor, foreColor;

W_Icon expview[EX_FRAMES];
W_Icon sbexpview[SBEXPVIEWS];
W_Icon cloud[NUMDETFRAMES];
W_Icon plasmacloud[NUMDETFRAMES];
W_Icon etorp, mtorp;
W_Icon eplasmatorp, mplasmatorp;
W_Icon shield, cloakicon, xsgbits;
W_Icon fed_bitmaps[NUM_TYPES][VIEWS],
       kli_bitmaps[NUM_TYPES][VIEWS],
       rom_bitmaps[NUM_TYPES][VIEWS],
       ori_bitmaps[NUM_TYPES][VIEWS],
       ind_bitmaps[NUM_TYPES][VIEWS] ;
W_Icon bplanets[6];
W_Icon mbplanets[6];
W_Icon bplanets2[8];
W_Icon mbplanets2[8];

char pseudo[PSEUDOSIZE];
char login[PSEUDOSIZE];


W_Window messagew, w, mapw, statwin, baseWin, infow, iconWin, war,
	warnw, helpWin, plstatw, reviewWin;
W_Window robotwin=0, optionWin = 0, modifyWin = 0;

int shipsallowed[NUM_TYPES];
int weaponsallowed[WP_MAX];
int startplanets[MAXPLANETS];
char godsname[32];	/* TSH 2/93 */

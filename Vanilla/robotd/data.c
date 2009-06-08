/*
 * data.c
 */
#include "copyright.h"

#include <stdio.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

struct player *players;
struct player *me=NULL;
struct torp *torps;
struct plasmatorp *plasmatorps;
struct status *status;
struct ship *myship;
struct stats *mystats;
struct planet *planets;
struct phaser *phasers;
struct message *messages;
struct mctl *mctl;
struct memory universe;
struct planetmatch plnamelist[MAXPLANETS];

int	inl = 0;

int	_master = 0;
int	oldalert = PFGREEN;	/* Avoid changing more than we have to */
int 	remap[16] = { 0, 1, 2, 0, 3, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0 };
int	messpend;
int	lastcount;
int	mdisplayed;
int	redrawall;
int	nopilot = 1;
int	selfdest;
int	_udcounter;
int	_udnonsync;
int	_cycletime;
int	_waiting_for_input;
double	_serverdelay;
double  _avsdelay;
int	_cycleroundtrip;
char	_server;
int	read_stdin=1;
int	override = 0;
int	doreserved = 0;
int	lastm;
int	delay;			/* delay for declaring war */
int	rdelay;			/* delay for refitting */
int	mapmode = 1; 
int	namemode = 1; 
int	showStats;
int	showShields;
int	warncount = 0;
int	warntimer = -1;
int	infomapped = 0;
int	mustexit = 0;
int	messtime = 5;
int	keeppeace = 0;
int	showlocal = 2;
int 	showgalactic = 2;
char 	*shipnos="0123456789abcdefghijklmnopqrstuvwxyz";
int	_master_sock = -1;
int 	sock= -1;
int	rsock = -1;
int	pollmode=0;
int	expltest=0;
char	rw_host[80];
int	xtrekPort=2592;
int	queuePos= -1;
int	pickOk= -1;
int	lastRank= -1;
int	promoted= 0;
int	loginAccept= -1;
unsigned localflags=0;
int	tournMask=15;
int 	nextSocket;	/* socket to use when we get ghostbusted... */
int	updatePlayer[MAXPLAYER];	/* Needs updating on player list */
char   *serverName="127.0.0.1";
int	loggedIn=0;
int 	reinitPlanets=0;
int	redrawPlayer[MAXPLAYER];	/* Needs redrawing on galactic map */
int	lastUpdate[MAXPLAYER]={0};	/* Last update of this player */
int     timerDelay=200000;		/* micro secs between updates */
int	reportKills=1;			/* report kill messages? */
float	updates=2.0;
int	nopwd=0;
int	randtorp=0;

#ifdef ATM
int     scanplayer;                     /* who to scan */
int     showTractor=1;                  /* show visible tractor beams */
int     commMode=0;                     /* UDP: 0=TCP only, 1=UDP updates */
int     commModeReq=0;                  /* UDP: req for comm protocol change */
int     commStatus=0;                   /* UDP: used when switching protocols */
int     commSwitchTimeout=0;            /* UDP: don't wait forever */
int     udpTotal=1;                     /* UDP: total #of packets received */
int     udpDropped=0;                   /* UDP: count of packets dropped */
int     udpRecentDropped=0;             /* UDP: #of packets dropped recently */
int     udpSock= -1;                     /* UDP: the socket */
int     udpDebug=0;                     /* UDP: debugging info on/off */
int     udpClientSend=1;                /* UDP: send our packets using UDP? */
int     udpClientRecv=1;                /* UDP: receive with fat UDP */
int     udpSequenceChk=1;               /* UDP: check sequence numbers */
#endif

int	recv_short = 0;

char teamlet[] = {'I', 'F', 'R', 'X', 'K', 'X', 'X', 'X', 'O'};
char *teamshort[9] = {"IND", "FED", "ROM", "X", "KLI", "X", "X", "X", "ORI"};
char pseudo[PSEUDOSIZE];
char login[PSEUDOSIZE];

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

int 		detall=0;
int		last_tract_req=0;
int		last_press_req=0;
unsigned char	curr_target_tcrs;

int		tractt_angle = 8;
int		tractt_dist = 4000;
int		shmem=0;
int		ogg_early_dist = 15000;
int		hit_by_torp=0;
int      	aim_in_dist = 6000;
int		ogg_offx, ogg_offy;
int		ogg_state;
int		evade_radius=5000;
int		cloak_odist=15000;
int		locked = 0;
int		phaser_int = 10;
int		explode_danger = 0;
int		last_udcounter = 0;
int		defend_det_torps;
int		no_tspeed = 0;

int             ping = 0;               /* to ping or not to ping */
long            packets_sent=0;         /* # all packets sent to server */
long            packets_received=0;     /* # all packets received */

int		no_cloak = 0;
int		oggv_packet=0;
int		off=0,def=0;

int             ignoreTMode = 0;
int             hm_cr = 0;  /* assume humans carry if they have kills */
int             ogg_happy = 0; /* ogg carriers while bombing */
int             robdc = 0; /* track robot carriers by default */

/*
 * data.h
 */
#include "copyright.h"

#define EX_FRAMES 		5
#define SBEXPVIEWS 		7
#define NUMDETFRAMES		5	/* # frames in torp explosion */

#define SERVER_LOCAL		0
#define SERVER_AUK		1
#define SERVER_BEZIER		2
#define SERVER_PITT		3
#define SERVER_FOGHORN		4
#define SERVER_NEEDMORE		5
#define SERVER_GRIT		6

extern int	ihypot();

extern struct player *players;
extern struct player *me;
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
extern struct memory universe;
extern struct planet pdata[];
extern struct planetmatch plnamelist[MAXPLANETS];

extern int	inl;

extern int 	_master;
extern int oldalert;
extern int remap[];
extern int 	_udcounter;
extern int 	_udnonsync;
extern int 	_cycletime;
extern double 	_serverdelay;
extern double 	_avsdelay;
extern int 	_cycleroundtrip;
extern int	_waiting_for_input;
extern char	_server;
extern int	read_stdin;
extern int	override;
extern int	doreserved;
extern int messpend;
extern int lastcount;
extern int mdisplayed;
extern int redrawall;
extern int nopilot;
extern int watch;
extern int selfdest;
extern int lastm;
extern int delay;
extern int rdelay;
extern int mapmode; 
extern int namemode; 
extern int showShields;
extern int showStats;
extern int warncount;
extern int warntimer;
extern int infomapped;
extern int mustexit;
extern int messtime;
extern int keeppeace;
extern int showlocal, showgalactic;
extern char *shipnos;
extern int _master_sock;
extern int sock;
extern int 	rsock;
extern int	pollmode;
extern int	expltest;
extern char	rw_host[];
extern int xtrekPort;
extern int queuePos;
extern int pickOk;
extern int lastRank;
extern int promoted;
extern int loginAccept;
extern unsigned localflags;
extern int tournMask;
extern int nextSocket;
extern int updatePlayer[];
extern char *serverName;
extern int loggedIn;
extern int reinitPlanets;
extern int redrawPlayer[];
extern int lastUpdate[];
extern int timerDelay;
extern int reportKills;
extern float updates;
extern int nopwd;
extern int randtorp;

#ifdef ATM
extern int scanplayer;
extern int showTractor;
extern int commMode;            /* UDP */
extern int commModeReq;         /* UDP */
extern int commStatus;          /* UDP */
extern int commSwitchTimeout;   /* UDP */
extern int udpTotal;            /* UDP */
extern int udpDropped;          /* UDP */
extern int udpRecentDropped;    /* UDP */
extern int udpSock;             /* UDP */
extern int udpDebug;            /* UDP */
extern int udpClientSend;       /* UDP */
extern int udpClientRecv;       /* UDP */
extern int udpSequenceChk;      /* UDP */
#endif

extern int recv_short;

extern double	Sin[], Cos[];

extern char teamlet[];
extern char *teamshort[];
extern char pseudo[PSEUDOSIZE];
extern char login[PSEUDOSIZE];

extern struct rank ranks[NUMRANKS];

extern int 		detall;
extern int		last_tract_req;
extern int		last_press_req;
extern unsigned char	curr_target_tcrs;
extern int		tractt_angle;
extern int		tractt_dist;
extern int		shmem;
extern int		ogg_early_dist;
extern int		hit_by_torp;
extern int		aim_in_dist;
extern int		ogg_offx, ogg_offy;
extern int		ogg_state;
extern int		evade_radius;
extern int		cloak_odist;
extern int		locked;
extern int		phaser_int;
extern int		explode_danger;
extern int		last_udcounter;
extern int		defend_det_torps;
extern int		no_tspeed;

extern int              ping;                   /* to ping or not to ping */
extern long             packets_sent;           /* # all packets sent to server
*/
extern long             packets_received;       /* # all packets received */

extern int		no_cloak;
extern int		oggv_packet;
extern int		off,def;

extern int              ignoreTMode;
extern int              hm_cr;      /* assume a human carries mode */
extern int              ogg_happy;  /* ogg close by carriers if bombing */
extern int              robdc;      /* robots don't carry, or don't track
                                       robot carriers */

/*this is also defined in ../include/data.h*/
#define PRE_T_ROBOT_LOGIN "Pre_T_Robot!"

/* $Id: data.c,v 1.7 2006/05/06 12:06:39 quozl Exp $
 */

#include "copyright.h"
#include "config.h"
#include "defs.h"
#include "struct.h"
#include "data.h"

struct player *players;
struct player *me;
struct torp *torps;
struct context *context;
struct status *status;
struct ship *myship;
struct stats *mystats;
struct planet *planets;
struct phaser *phasers;
struct mctl *mctl;
struct message *messages;
struct message *uplink;
struct message *oob;
struct team *teams;
struct ship *shipvals;
struct pqueue *queues;
struct queuewait *waiting;
struct ban *bans;

int	oldalert = PFGREEN;	/* Avoid changing more than we have to */

int	remap[9] = { 0, 1, 2, -1, 3, -1, -1, -1, 4 };
int	fps;		/* simulation frames per second */
int	distortion;	/* time distortion constant, 10 is normal */
int	minups;		/* minimum client updates per second */
int	defups;		/* default client updates per second */
int	maxups;		/* maximum client updates per second */
int	messpend;
int	lastcount;
int	mdisplayed;
int	redrawall;
int	udcounter;
int	lastm;
int	delay;			/* delay for decaring war */
int	rdelay;			/* delay for refitting */
int	doosher = 0;		/* NBT 11-4-92 print doosh messages */
int	dooshignore = 0;	/* NBT 3/30/92 ignore doosh messages */
int	macroignore = 0;
int	whymess = 0;		/* NBT 6/1/93  improved why dead messages */
int	show_sysdef=0;		/* NBT 6/1/93  show sysdef on galactic */
int	max_pop=999;		/* NBT 9/9/93  sysdef for max planet pop */
int	mapmode = 1; 
int	namemode = 1; 
int	showStats;
int	showShields;
int	warncount = 0;
int	warntimer = -1;
int	infomapped = 0;
int	mustexit = 0;
int	messtime = MESSTIME;
int	keeppeace = 0;
int	showlocal = 2;
int 	showgalactic = 2;
char 	*shipnos="0123456789abcdefghijklmnopqrstuvwxyz";
int 	sock= -1;
int 	xtrekPort=27320;
int	shipPick;
int	teamPick;
int	repCount=0;
char 	namePick[NAME_LEN];
char 	passPick[NAME_LEN];
fd_set	inputMask;
int 	nextSocket;
char	*host;
char	*ip;
int	userVersion=0, userUdpVersion=0;
int	bypassed=0;
int	top_armies=30;		/*initial army count default */
int	errorlevel=1;		/* controlling amount of error info */
int	dead_warp=0;		/* use warp 14 for death detection */
int	surrenderStart=1;	/* # of planets to start surrender counter */
int	sbplanets=5;		/* # of planets to get a base */
int	sborbit=0;		/* Disallow bases to orbit enemy planets */
int	restrict_3rd_drop=0;	/* Disallow dropping on 3rd space planets */
int	restrict_bomb=1;        /* Disallow bombing outside of T-Mode */
int	no_unwarring_bombing=1; /* No 3rd space bombing */

char *shipnames[NUM_TYPES] = {
      "Scout", "Destroyer", "Cruiser", "Battleship", 
      "Assault", "Starbase", "SGalaxy", "AT&T"
};
char *shiptypes[NUM_TYPES] = {"SC", "DD", "CA", "BB", "AS", "SB", "GA", "AT"}; 
char *weapontypes[WP_MAX] = {"PLASMA", "TRACTOR"};
char *planettypes[MAXPLANETS] = {
    "00", "01", "02", "03", "04", "05", "06", "07", "08", "09",
    "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
    "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
    "30", "31", "32", "33", "34", "35", "36", "37", "38", "39"
};


#ifdef RSA			/* NBT added */
char    RSA_client_type[256];   /* from LAB 4/15/93 */
char	testdata[KEY_SIZE];
int	RSA_Client;
#else
char	testdata[NAME_LEN];
#endif

int     checkmessage = 0;
int     logall=0;
int     loggod=0;
int   eventlog=0;

int	manager_type = 0;
int	manager_pid = 0;

int	why_dead=0;
int	Observer=0;
int	switch_to_observer=0;
int	switch_to_player=0;
int	SBhours=0;

int	testtime= -1;
int 	tournplayers=5;
int	safe_idle=0;
int 	shipsallowed[NUM_TYPES];
int 	weaponsallowed[WP_MAX];
int 	plkills=2;
int	ddrank=0;
int	garank=0;
int 	sbrank=3;
int 	startplanets[MAXPLANETS];
int 	binconfirm=1;
int	chaosmode=0;
int	topgun=0; /* added 12/9/90 TC */
int	nodiag=1;
int	classictourn=0;
int	report_users=1;
int	killer=1; /* added 1/28/93 NBT */
int	resetgalaxy=0; /* added 2/6/93 NBT */
int     newturn=0; /* added 1/20/91 TC */
int	planet_move=0;	/* added 9/28/92 NBT */
int	RSA_confirm=0;  /* added 10/18/92 NBT */
int	check_scum=0;	/* added 7/21/93 NBT */
int	wrap_galaxy=0; /* added 6/29/95 JRP */
int	pingpong_plasma=0; /* added 7/6/95 JRP */
int	max_chaos_bases=2; /* added 7/7/95 JRP */
int	errors;	/*fd to try and fix stderrs being sent over socket NBT 9/23/93*/
int	hourratio=1;	/* Fix thing to make guests advance fast */
int	minRank=0;         /* isae - Minimal rank allowed to play */
int	clue=0;		/* NBT - clue flag for time_access() */
int	cluerank=6;
int	clueVerified=0;
int     clueFuse=0;
int     clueCount=0;
char	*clueString;
int	hiddenenemy=1;
int	vectortorps = 0;
int	galactic_smooth = 0;

int	udpSock=(-1);     /* UDP - the almighty socket */
int	commMode=0;     /* UDP - initial mode is TCP only */
int	udpAllowed=1;   /* UDP - sysdefaults */
int	bind_udp_port_base=0;

double	oldmax = 0.0;

extern double	Sin[], Cos[];		 /* Initialized in sintab.c */

char	teamlet[] = {'I', 'F', 'R', 'X', 'K', 'X', 'X', 'X', 'O'};
char	*teamshort[9] = {"IND", "FED", "ROM", "X", "KLI", "X", "X", "X", "ORI"};
char	login[PSEUDOSIZE];

struct rank ranks[NUMRANKS] = {
    { 0.0, 0.0, 0.0, "Ensign", "Esgn"},
    { 2.0, 1.0, 0.0, "Lieutenant", "Lt  "},
    { 4.0, 2.0, 0.0, "Lt. Cmdr.", "LtCm"},
    { 8.0, 3.0, 0.0, "Commander", "Cder"},
    {15.0, 4.0, 0.0, "Captain", "Capt"},
    {20.0, 5.0, 0.0, "Flt. Capt.", "FltC"},

/* change: 6/2/07 KA turn the defense measure into offense
   to challenge those players who just come in for the
   initial bombing and planet taking scumming; this is
   intended to improve team play amongst such players */
    {25.0, 6.0, 1.0, "Commodore", "Cdor"},
    {30.0, 7.0, 1.2, "Rear Adm.", "RAdm"},
    {40.0, 8.0, 1.4, "Admiral", "Admr"}};

int             startTkills, startTlosses, startTarms, startTplanets,
                startTticks;

int		startSBkills, startSBlosses, startSBticks;

int     ping=0;                 /* to ping or not to ping, client's decision*/
LONG    packets_sent=0;         /* # all packets sent to client */
LONG    packets_received=0;     /* # all packets received from client */
int     ping_ghostbust=0;       /* ghostbust timer */

/* defaults variables */
int     ping_freq=1;            /* ping freq. in seconds */
int     ping_iloss_interval=10; /* in values of ping_freq */
int     ping_allow_ghostbust=0; /* allow ghostbust detection from
                                   ping_ghostbust (cheating possible)*/
int     ping_ghostbust_interval=5; /* in ping_freq, when to ghostbust
                                      if allowed */

int	ghostbust_timer=30;

int           twarpMode=0;       /* isae - SB transwarp */
int           twarpSpeed=60;     /* isae - Warp speed */
int           twarpDelay=1;      /* quozl - start delay in milliseconds */

int send_short = 0;
int send_threshold = 0;		/* infinity */
int actual_threshold = 0;	/* == send_threshold / numupdates */
int numupdates = 5;		/* For threshold */

int adminexec = 0;
char admin_password[ADMIN_PASS_LEN];
char script_tourn_start[FNAMESIZE];
char script_tourn_end[FNAMESIZE];

char Global[FNAMESIZE];
char Scores[FNAMESIZE];
char PlFile[FNAMESIZE];
char Motd[FNAMESIZE] = N_MOTD ;
char Motd_Path[FNAMESIZE];
char Daemon[FNAMESIZE];
char Robot[FNAMESIZE];
char LogFileName[FNAMESIZE];
char PlayerFile[FNAMESIZE];
char PlayerIndexFile[FNAMESIZE];
char ConqFile[FNAMESIZE];
char SysDef_File[FNAMESIZE];
char Time_File[FNAMESIZE];
char Clue_Bypass[FNAMESIZE];
char Banned_File[FNAMESIZE];
char Scum_File[FNAMESIZE];
char Error_File[FNAMESIZE];
char Bypass_File[FNAMESIZE];
#ifdef RSA
char RSA_Key_File[FNAMESIZE];
#endif
#ifdef AUTOMOTD
char MakeMotd[FNAMESIZE];
#endif
char MesgLog[FNAMESIZE];
char GodLog[FNAMESIZE];
char Feature_File[FNAMESIZE];
#ifdef BASEPRACTICE
char Basep[FNAMESIZE];
#endif
#ifdef NEWBIESERVER
char Newbie[FNAMESIZE];
int newbie_balance_humans=1;
int min_newbie_slots=12;
int max_newbie_players=8;
#endif
#ifdef PRETSERVER
char PreT[FNAMESIZE];
int pret_guest = 0;
int pret_planets = 3;
int pret_save_galaxy = 1;
int pret_galaxy_lifetime = 600;
int pret_save_armies = 1;
#endif
int robot_debug_target = -1;
int robot_debug_level = 0;
#if defined(BASEPRACTICE) || defined(NEWBIESERVER)  || defined(PRETSERVER)
char Robodir[FNAMESIZE];
char robofile[FNAMESIZE];
char robot_host[FNAMESIZE];
int is_robot_by_host = 1;
#endif
#ifdef DOGFIGHT
char Mars[FNAMESIZE];
#endif
char Puck[FNAMESIZE];
char Inl[FNAMESIZE];
int inl_record = 0;
int inl_draft_style = 0;

char NoCount_File[FNAMESIZE];
char Prog[FNAMESIZE];
char LogFile[FNAMESIZE];

#ifdef DOGFIGHT
/* Needed for Mars */
int   nummatch=4;
int   contestsize=4;
int     dogcounts=0;
#endif

char Cambot[FNAMESIZE];
char Cambot_out[FNAMESIZE];

int num_distress;

/* the following copied out of BRM data.c */

int num_distress;

     /* the index into distmacro array should correspond 
   	with the correct dist_type */
struct  dmacro_list distmacro[] = {
	{ 'X', "no zero", "this should never get looked at" },
	{ 't', "taking", " %T%c->%O (%S) Carrying %a to %l%?%n>-1%{ @ %n%}\0"},
	{ 'o', "ogg", " %T%c->%O Help Ogg %p at %l\0" },
	{ 'b', "bomb", " %T%c->%O %?%n>4%{bomb %l @ %n%!bomb %l%}\0"},
	{ 'c', "space_control", " %T%c->%O Help Control at %L\0" },
	{ '1', "save_planet", " %T%c->%O Emergency at %L!!!!\0" },
	{ '2', "base_ogg", " %T%c->%O Sync with --]> %g <[-- OGG ogg OGG base!!\0" },
	{ '3', "help1", " %T%c->%O Help me! %d%% dam, %s%% shd, %f%% fuel %a armies.\0" },
	{ '4', "help2", " %T%c->%O Help me! %d%% dam, %s%% shd, %f%% fuel %a armies.\0" },
	{ 'e', "escorting", " %T%c->%O ESCORTING %g (%d%%D %s%%S %f%%F)\0" },
	{ 'p', "ogging", " %T%c->%O Ogging %h\0" },
	{ 'm', "bombing", " %T%c->%O Bombing %l @ %n\0" },
	{ 'l', "controlling", " %T%c->%O Controlling at %l\0" },
	{ '5', "asw", " %T%c->%O Anti-bombing %p near %b.\0" } ,
	{ '6', "asbomb", " %T%c->%O DON'T BOMB %l. Let me bomb it (%S)\0" } ,
	{ '7', "doing1", " %T%c->%O (%i)%?%a>0%{ has %a arm%?%a=1%{y%!ies%}%} at %l.  (%d%% dam, %s%% shd, %f%% fuel)\0" } ,
	{ '8', "doing2", " %T%c->%O (%i)%?%a>0%{ has %a arm%?%a=1%{y%!ies%}%} at %l.  (%d%% dam, %s%% shd, %f%% fuel)\0" } ,
	{ 'f', "free_beer", " %T%c->%O %p is free beer\0" },
	{ 'n', "no_gas", " %T%c->%O %p @ %l has no gas\0" },
	{ 'h', "crippled", " %T%c->%O %p @ %l crippled\0" },
	{ '9', "pickup", " %T%c->%O %p++ @ %l\0" },
	{ '0', "pop", " %T%c->%O %l%?%n>-1%{ @ %n%}!\0"},
	{ 'F', "carrying", " %T%c->%O %?%S=SB%{Your Starbase is c%!C%}arrying %?%a>0%{%a%!NO%} arm%?%a=1%{y%!ies%}.\0" },
	{ '@', "other1", " %T%c->%O (%i)%?%a>0%{ has %a arm%?%a=1%{y%!ies%}%} at %l. (%d%%D, %s%%S, %f%%F)\0" },
	{ '#', "other2", " %T%c->%O (%i)%?%a>0%{ has %a arm%?%a=1%{y%!ies%}%} at %l. (%d%%D, %s%%S, %f%%F)\0" },
	{ 'E', "help", " %T%c->%O Help(%S)! %s%% shd, %d%% dmg, %f%% fuel,%?%S=SB%{ %w%% wtmp,%!%}%E%{ ETEMP!%}%W%{ WTEMP!%} %a armies!\0" },
	{ '\0', '\0', '\0'},
};



/*------------------------------------------------------------------------

 Stolen direct from the Paradise 2.2.6 server code 
 This code is used to set shipvalues from .sysdef

 Added 1/9/94 by Steve Sheldon
------------------------------------------------------------------------*/

#define OFFSET_OF(field)        ( (char*)(&((struct ship*)0)->field) -\
                          (char*)0)

struct field_desc ship_fields[] = {
	{"turns",		FT_INT,   OFFSET_OF (s_turns)},
	{"accs",		FT_SHORT, OFFSET_OF (s_accs)},
	{"torpdamage",		FT_SHORT, OFFSET_OF (s_torpdamage)},
	{"phaserdamage",	FT_SHORT, OFFSET_OF (s_phaserdamage)},
	{"phaserfuse",		FT_SHORT, OFFSET_OF (s_phaserfuse)},
	{"plasmadamage",	FT_SHORT, OFFSET_OF (s_plasmadamage)},
	{"torpspeed",		FT_SHORT, OFFSET_OF (s_torpspeed)},
	{"torpfuse",		FT_SHORT, OFFSET_OF (s_torpfuse)},
	{"torpturns",		FT_SHORT, OFFSET_OF (s_torpturns)},
	{"plasmaspeed",		FT_SHORT, OFFSET_OF (s_plasmaspeed)},
	{"plasmafuse",		FT_SHORT, OFFSET_OF (s_plasmafuse)},
	{"plasmaturns",		FT_SHORT, OFFSET_OF (s_plasmaturns)},
	{"maxspeed",		FT_INT,   OFFSET_OF (s_maxspeed)},
	{"repair",              FT_SHORT, OFFSET_OF (s_repair)},
	{"maxfuel",		FT_INT,   OFFSET_OF (s_maxfuel)},
	{"torpcost",		FT_SHORT, OFFSET_OF (s_torpcost)},
	{"plasmacost",		FT_SHORT, OFFSET_OF (s_plasmacost)},
	{"phasercost",		FT_SHORT, OFFSET_OF (s_phasercost)},
	{"detcost",		FT_SHORT, OFFSET_OF (s_detcost)},
	{"warpcost",		FT_SHORT, OFFSET_OF (s_warpcost)},
	{"cloakcost",		FT_SHORT, OFFSET_OF (s_cloakcost)},
	{"recharge",		FT_SHORT, OFFSET_OF (s_recharge)},
	{"accint",		FT_INT,   OFFSET_OF (s_accint)},
	{"decint",		FT_INT,   OFFSET_OF (s_decint)},
	{"maxshield",		FT_INT,   OFFSET_OF (s_maxshield)},
	{"maxdamage",		FT_INT,   OFFSET_OF (s_maxdamage)},
	{"maxegntemp",		FT_INT,   OFFSET_OF (s_maxegntemp)},
	{"maxwpntemp",		FT_INT,   OFFSET_OF (s_maxwpntemp)},
	{"egncoolrate",		FT_SHORT, OFFSET_OF (s_egncoolrate)},
	{"wpncoolrate",		FT_SHORT, OFFSET_OF (s_wpncoolrate)},
	{"maxarmies",		FT_SHORT, OFFSET_OF (s_maxarmies)},
	{"width",		FT_SHORT, OFFSET_OF (s_width)},
	{"height",		FT_SHORT, OFFSET_OF (s_height)},
	{"type",		FT_SHORT, OFFSET_OF (s_type)},
	{"mass",		FT_SHORT, OFFSET_OF (s_mass)},
	{"tractstr",		FT_SHORT, OFFSET_OF (s_tractstr)},
	{"tractrng",		FT_FLOAT, OFFSET_OF (s_tractrng)},
	{0}
};

int F_client_feature_packets = 0;
int F_ship_cap      = 0;
int F_cloak_maxwarp = 0;
int F_rc_distress   = 0;
int F_self_8flags   = 0;
int F_19flags       = 0;	/* pack 19 flags into spare bytes */
int F_show_all_tractors = 0;
int F_sp_generic_32 = 0;		/* Repair time, etc. */
int A_sp_generic_32 = 0;
int F_full_direction_resolution = 0;	/* Use SP_PLAYER instead of
					 * SP_S_PLAYER */
int F_full_weapon_resolution = 0;	/* Send certain weapons data
					 * beyond tactical range */
int F_check_planets = 0;		/* allow client to confirm
					 * planet info is correct */
int F_show_army_count = 0;		/* army count by planet */
int F_show_other_speed = 0;		/* show other player's speed */
int F_show_cloakers = 0;		/* show other cloakers on tactical */
int F_turn_keys = 0;			/* use keyboard to turn */
int F_show_visibility_range = 0;	/* allow client to show range at
					 * which enemies can see self */
int F_flags_all = 0;			/* SP_FLAGS_ALL packet may be sent */
int F_why_dead_2 = 0;			/* Use additional p_whydead states */
int F_auto_weapons = 0;			/* allow client to autoaim/fire */
int F_sp_rank = 0;			/* send rank_spacket */
int F_sp_ltd = 0;			/* send ltd_spacket */
int F_tips = 0;				/* supports motd clearing and tips */
int mute = 0;
int muteall = 0;
int remoteaddr = -1;		/* inet address in net format */
int observer_muting = 0;
int observer_keeps_game_alive = 0;
int whitelisted = 0;
int blacklisted = 0;
int hidden = 0;
int ignored[MAXPLAYER];

int voting=0;
int ban_vote_enable=0;
int ban_vote_duration=3600;
int ban_noconnect=0;
int eject_vote_enable=0;
int eject_vote_only_if_queue=0;
int eject_vote_vicious=0;
int nopick_vote_enable=0;
int duplicates=3;
int deny_duplicates=0;
int blogging=1;
int genoquit=0;

#ifdef STURGEON
int sturgeon = 0;
int sturgeon_special_weapons = 1;
int sturgeon_maxupgrades = 100;
int sturgeon_extrakills = 0;
int sturgeon_planetupgrades = 0;
int sturgeon_lite = 0;
int upgradeable = 1;
char *upgradename[] = { "misc", "shield", "hull", "fuel", "recharge", "maxwarp",
                       "accel", "decel", "engine cool", "phaser damage",
                       "torp speed", "torp fuse", "weapon cooling", "cloak", "tractor str",
                       "tractor range", "repair", "fire while cloaked", "det own torps for damage" };
double baseupgradecost[] = { 0.0, 1.0, 1.5, 0.5, 1.0, 2.0, 0.5, 0.5, 1.0, 1.0,
                             2.0, 0.5, 1.0, 2.0, 1.0, 1.0, 1.0, 5.0, 5.0, 0.0 };
double adderupgradecost[]= { 0.0, 0.2, 0.5, 0.1, 1.5, 1.0, 0.1, 0.1, 0.5, 1.5,
                             3.0, 0.5, 1.5, 1.0, 0.5, 1.5, 1.0, 0.0, 0.0, 0.0 };
#endif

/* starbase rebuild time first implemented by TC in 1992-04-15 */
int starbase_rebuild_time = 30;

/* size of most recent UDP packet sent, for use by udpstats command */
int last_udp_size = 0;

int ip_check_dns = 0;
int ip_check_dns_verbose = 0;

int offense_rank = 0;

/* allow whitelisted entries to bypass ignore settings */
int whitelist_indiv = 0;
int whitelist_team = 0;
int whitelist_all = 0;

int server_advertise_enable = 0;
char server_advertise_filter[MSG_LEN];

float as_minimal_offense = 0.0;
float bb_minimal_offense = 0.0;
float dd_minimal_offense = 0.0;
float sb_minimal_offense = 0.0;
float sc_minimal_offense = 0.0;

/* classical style refit at < 75% hull damage */
int lame_refit = 1;
/* classical style base refit at < 75% hull damage */
int lame_base_refit = 1;

/* login time, in seconds, default set from Netrek XP in 2008-06 */
int logintime = 100;

/* whether self destructing gives credit to nearest enemy */
int self_destruct_credit = 0;

/* Whether to report the client identification of arriving players */
int report_ident = 0;

int has_set_speed = 0;
int has_set_course = 0;
int has_shield_up = 0;
int has_shield_down = 0;
int has_bombed = 0;
int has_beamed_up = 0;
int has_beamed_down = 0;
int has_repaired = 0;

/* Asstorp penalties.  Adjust values will be a mutiplier of this value, so
 * a value of 1.0 gives the standard bronco behaviour of "no changes" */
float asstorp_fuel_mult = 1.0;
float asstorp_etemp_mult = 1.0;
float asstorp_wtemp_mult = 1.0;
int asstorp_base = 0;

int recreational_dogfight_mode = 0;
int planet_plague = 1;
int self_reset = 1;

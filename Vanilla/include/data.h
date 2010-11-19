/* $Id: data.h,v 1.5 2006/04/26 09:52:43 quozl Exp $
 */

#ifndef _h_data
#define _h_data


/* data.h
 */
#include "copyright.h"

#define EX_FRAMES 		5
#define SBEXPVIEWS 		7
#define ex_width        	64
#define ex_height       	64
#define sbexp_width        	80
#define sbexp_height       	80
#define cloud_width 		7
#define cloud_height 		7
#define plasmacloud_width 	11
#define plasmacloud_height 	11
#define etorp_width 		3
#define etorp_height 		3
#define eplasmatorp_width 	7
#define eplasmatorp_height 	7
#define mplasmatorp_width 	5
#define mplasmatorp_height 	5
#define mtorp_width 		3
#define mtorp_height 		3
#define crossw		 	16
#define crossh		 	16
#define cross_x_hot	 	7
#define cross_y_hot	 	7
#define crossmask_width 	16
#define crossmask_height 	16
#define planet_width 		30
#define planet_height 		30
#define mplanet_width 		16
#define mplanet_height 		16
#define shield_width 		20
#define shield_height 		20
#define icon_width 		112
#define icon_height 		80
#define att_gray_mask_width     28
#define att_gray_mask_height    28
#define gray_mask_width         20
#define gray_mask_height        20

extern int minRank; /* isae */
extern int clue;
extern int cluerank;
extern int clueVerified;
extern int clueFuse;
extern int clueCount;
extern char *clueString;

extern struct player *players;
extern struct player *me;
extern struct torp *torps;
extern struct context *context;
extern struct status *status;
extern struct ship *myship;
extern struct stats *mystats;
extern struct planet *planets;
extern struct phaser *phasers;
extern struct mctl *mctl;
extern struct message *messages;
extern struct message *uplink;
extern struct message *oob;
extern struct team *teams;
extern struct ship *shipvals;
extern struct pqueue *queues;
extern struct queuewait *waiting;
extern struct ban *bans;

extern int	oldalert;
extern int	remap[];
extern int	udcounter;
extern int	fps;		/* simulation frames per second */
extern int	distortion;	/* time distortion constant, 10 is normal */
extern int	minups;		/* minimum client updates per second */
extern int	defups;		/* default client updates per second */
extern int	maxups;		/* maximum client updates per second */
extern int	messpend;
extern int	lastcount;
extern int	mdisplayed;
extern int	redrawall;
extern int	lastm;
extern int	delay;
extern int	rdelay;
extern int	doosher;		/* NBT 11-4-92 */
extern int	dooshignore;		/* NBT 3-30-93 */
extern int	macroignore;
extern int	whymess;		/* NBT 6-1-93  */
extern int	show_sysdef;		/* NBT 6-1-93  */
extern int	max_pop;		/* NBT 9-9-93  */
extern int	mapmode; 
extern int	namemode; 
extern int	showShields;
extern int	showStats;
extern int	warncount;
extern int	warntimer;
extern int	infomapped;
extern int	mustexit;
extern int	messtime;
extern int	keeppeace;
extern int	showlocal, showgalactic;
extern char	*shipnos;
extern int	sock;
extern int	xtrekPort;
extern int	shipPick;
extern int	teamPick;
extern int	repCount;
extern char	namePick[];
extern char	passPick[];
extern fd_set	inputMask;
extern int	nextSocket;
extern char	*host;
extern char	*ip;
extern int	userVersion, userUdpVersion;
extern int	clue;
extern int	clueverified;
extern char	cluestring[50];
extern int	clueinterval;
extern int	cluedelay;
extern int	bypassed;
extern int	top_armies;            /* 11/27/93 ATH */
extern int	errorlevel;
extern int	dead_warp;
extern int	surrenderStart;
extern int	sbplanets;
extern int	sborbit;
extern int	restrict_3rd_drop;
extern int	restrict_bomb;
extern int	no_unwarring_bombing;


#define WP_PLASMA 0
#define WP_TRACTOR 1
#define WP_MAX 2

#define VIEWS 16
#define NUM_TYPES 8

extern char *shipnames[NUM_TYPES];
extern char *shiptypes[NUM_TYPES];
extern char *weapontypes[WP_MAX];
extern char *planettypes[MAXPLANETS];


#ifdef RSA
extern char testdata[KEY_SIZE];
extern char RSA_client_type[256];	/* from LAB 4/15/93 */
extern int RSA_Client;
#else
extern char testdata[16];
#endif

extern int manager_type;
extern int manager_pid;

extern int why_dead;
extern int Observer;
extern int switch_to_observer;
extern int switch_to_player;
extern int SBhours;

extern int	checkmessage;
extern int	logall;
extern int	loggod;
extern int	eventlog;

extern int testtime;
extern int tournplayers;
extern int safe_idle;
extern int shipsallowed[];
extern int weaponsallowed[];
extern struct field_desc ship_fields[];
extern int plkills;
extern int ddrank;
extern int garank;
extern int sbrank;
extern int startplanets[];
extern int binconfirm;
extern int chaosmode;
extern int topgun; /* added 12/9/90 TC */
extern int nodiag;
extern int classictourn;
extern int report_users;
extern int killer; /* added 1/28/93 NBT */
extern int resetgalaxy; /* added 2/6/93 NBT */
extern int newturn; /* added 1/20/91 TC */
extern int planet_move;	/* added 9/28/92 NBT */
extern int errors;	/* added 2/23/92 NBT */
extern int check_scum;	/* added 7/21/93 NBT */
extern int wrap_galaxy; /* added 6/29/95 JRP */
extern int pingpong_plasma; /* added 7/6/95 JRP */
extern int max_chaos_bases; /* added 7/7/95 JRP */
extern int RSA_confirm;
extern int hourratio;
extern int hiddenenemy;
extern int vectortorps;
extern int galactic_smooth;

extern int udpSock;		/* UDP */
extern int commMode;		/* UDP */
extern int udpAllowed;		/* UDP */
extern int bind_udp_port_base;

extern double	oldmax;
extern double	Sin[], Cos[];

extern char teamlet[];
extern char *teamshort[];
extern char login[PSEUDOSIZE];

extern struct rank ranks[NUMRANKS];

extern int startTkills, startTlosses, startTarms, startTplanets,
           startTticks;

extern int startSBkills, startSBlosses, startSBticks;

extern int  ping;               /* to ping or not to ping */
extern LONG packets_sent;       /* # all packets sent to client */
extern LONG packets_received;   /* # all packets recved from client */
extern int  ping_ghostbust;     /* ghostbust timer */

/* defaults variables */
extern int  ping_freq;            /* ping freq. in seconds */
extern int  ping_iloss_interval;  /* in values of ping_freq */
extern int  ping_allow_ghostbust; /* allow ghostbust detection
                                     from 'ping_ghostbust'..
                                     (cheating possible) */
extern int  ping_ghostbust_interval; /* in ping_freq, when to ghostbust
                                        if allowed */

extern int ghostbust_timer;

extern int twarpMode;       /* isae - SB transwarp */
extern int twarpSpeed;     /* isae - Warp speed */
extern int twarpDelay;
extern int send_short;
extern int send_threshold;
extern int actual_threshold;
extern int numupdates;
extern int send_short;

extern int adminexec;
extern char admin_password[ADMIN_PASS_LEN];
extern char script_tourn_start[FNAMESIZE];
extern char script_tourn_end[FNAMESIZE];

extern char Global[FNAMESIZE];
extern char Scores[FNAMESIZE];
extern char PlFile[FNAMESIZE];
extern char Motd[FNAMESIZE];
extern char Motd_Path[FNAMESIZE];
extern char Daemon[FNAMESIZE];
extern char Robot[FNAMESIZE];
extern char LogFileName[FNAMESIZE];
extern char PlayerFile[FNAMESIZE];
extern char PlayerIndexFile[FNAMESIZE];
extern char ConqFile[FNAMESIZE];
extern char SysDef_File[FNAMESIZE];
extern char Time_File[FNAMESIZE];
extern char Clue_Bypass[FNAMESIZE];
extern char Banned_File[FNAMESIZE];
extern char Scum_File[FNAMESIZE];
extern char Error_File[FNAMESIZE];
extern char Bypass_File[FNAMESIZE];
#ifdef RSA
extern char RSA_Key_File[FNAMESIZE];
#endif
#ifdef AUTOMOTD
extern char MakeMotd[FNAMESIZE];
#endif
extern char MesgLog[FNAMESIZE];
extern char GodLog[FNAMESIZE];
extern char Feature_File[FNAMESIZE];
#ifdef BASEPRACTICE
extern char Basep[FNAMESIZE];
#endif
#ifdef NEWBIESERVER
extern char Newbie[FNAMESIZE];
extern int min_newbie_slots;
extern int max_newbie_players;
extern int newbie_balance_humans;
#endif
#ifdef PRETSERVER
extern char PreT[FNAMESIZE];
extern int pret_guest;
extern int pret_planets;
extern int pret_save_galaxy;
extern int pret_galaxy_lifetime;
extern int pret_save_armies;
#endif
extern int robot_debug_target;
extern int robot_debug_level;
#if defined(BASEPRACTICE) || defined(NEWBIESERVER) || defined(PRETSERVER)
extern char Robodir[FNAMESIZE];
extern char robofile[FNAMESIZE];
extern char robot_host[FNAMESIZE];
extern int is_robot_by_host;
#endif
#ifdef DOGFIGHT
extern char Mars[FNAMESIZE];
#endif
extern char Puck[FNAMESIZE];
extern char Inl[FNAMESIZE];
extern int inl_record;
extern int inl_draft_style;

extern char NoCount_File[FNAMESIZE];
extern char Prog[FNAMESIZE];
extern char LogFile[FNAMESIZE];

#ifdef DOGFIGHT
extern int    nummatch;
extern int    contestsize;
extern int      dogcounts;
#endif

extern int      num_distress;

/* copied from BRM data.h */
extern int      num_distress;
extern struct dmacro_list distmacro[];

extern int F_client_feature_packets;
extern int F_ship_cap;
extern int F_cloak_maxwarp;
extern int F_rc_distress;
extern int F_self_8flags;
extern int F_19flags;
extern int F_show_all_tractors;
extern int F_sp_generic_32;
extern int A_sp_generic_32;
extern int F_full_direction_resolution;
extern int F_full_weapon_resolution;
extern int F_check_planets;
extern int F_show_army_count;
extern int F_show_other_speed;
extern int F_show_cloakers;
extern int F_turn_keys;
extern int F_show_visibility_range;
extern int F_flags_all;
extern int F_why_dead_2;
extern int F_auto_weapons;
extern int F_sp_rank;
extern int F_sp_ltd;
extern int F_tips;

extern char Cambot[FNAMESIZE];
extern char Cambot_out[FNAMESIZE];

/*this is also defined in ../robotd/data.h*/
#define PRE_T_ROBOT_LOGIN "Pre_T_Robot!"

extern int mute;
extern int muteall;
extern int remoteaddr;		/* inet address in net format */
extern int observer_muting;
extern int observer_keeps_game_alive;
extern int whitelisted;
extern int blacklisted;
extern int hidden;

extern int voting;
extern int ban_vote_enable;
extern int ban_vote_duration;
extern int ban_noconnect;
extern int eject_vote_enable;
extern int eject_vote_only_if_queue;
extern int eject_vote_vicious;
extern int nopick_vote_enable;
extern int duplicates;
extern int deny_duplicates;
extern int blogging;
extern int genoquit;

#ifdef STURGEON
#define SPECPLAS 1
#define SPECBOMB 2
#define SPECTORP 3
#define SPECPHAS 4
#define SPECMINE 5
extern int sturgeon;
extern int sturgeon_special_weapons;
extern int sturgeon_maxupgrades;
extern int sturgeon_extrakills;
extern int sturgeon_planetupgrades;
extern int sturgeon_lite;
extern int upgradeable;
extern char *upgradename[];
extern double baseupgradecost[];
extern double adderupgradecost[];
#endif
extern int starbase_rebuild_time;
extern int last_udp_size;
extern int ip_check_dns;
extern int ip_check_dns_verbose;
extern int offense_rank;
extern int server_advertise_enable;
extern char server_advertise_filter[MSG_LEN];
extern int whitelist_indiv;
extern int whitelist_team;
extern int whitelist_all;
extern float as_minimal_offense;
extern float bb_minimal_offense;
extern float dd_minimal_offense;
extern float sb_minimal_offense;
extern float sc_minimal_offense;
extern int lame_refit;
extern int lame_base_refit;
extern int logintime;
extern int self_destruct_credit;

/* lame hack to get around gcc silliness */
extern double round(double);

extern int report_ident;

extern int has_set_speed;
extern int has_set_course;
extern int has_shield_up;
extern int has_shield_down;
extern int has_bombed;
extern int has_beamed_up;
extern int has_beamed_down;
extern int has_repaired;

extern float asstorp_fuel_mult;
extern float asstorp_etemp_mult;
extern float asstorp_wtemp_mult;
extern int asstorp_base;

extern int recreational_dogfight_mode;

extern int planet_plague;

extern int self_reset;

#endif /* _h_data */

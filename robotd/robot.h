
#define PASS		"t"
#define PASS2		"tover"
#define bool		int

extern unsigned char   get_course(),
		       get_acourse(),
		       get_awrapcourse();
extern struct planet	*find_safe_planet(), *nearest_safe_planet();
extern char            *state_name();
extern char		team_bit();
extern struct planet   *name_to_planet(), *home_planet();
extern unsigned char 	choose_course();

extern int		_cycletime;
extern int		_udcounter;

extern struct planet	*find_planet_by_shortname(),
			*find_planet_by_longname();

extern struct _player	*me_p;

typedef struct  _player {
   int			last_init;
   unsigned int         pp_flags;
   struct player        *p;

   int			dist, lastdist, hispr;
   unsigned char	crs, icrs;

   char                 robot;
   char			enemy;
   char			invisible;

   int			run_t;
   int			fuel, subshield, shield, wpntemp, subdamage, damage;
   int			armies, pl_armies, beam_fuse;
   int			killfuse;    /* how long with kills */
   float		plcarry;
   int			last_comm_beamup;
   /* last position */
   int			p_x, p_y;
   int			alive;
   /* cloak information */
   unsigned char	lastphdir, lasttdir;
   int			phit, thit, thittime, phittime;
   int			cx,cy;
   int			phit_me, phittime_me;

   bool			sb_dontuse;
 
   struct _player	*attacking[MAXPLAYER];
   int			distances[MAXPLAYER];
   int			server_update;		/* udcounter */

   struct planet	*closest_pl;
   int			closest_pl_dist;
   /* timers */
   int			bombing, taking;

   /* cyborg detectors */

   /* torp fire time, phaser fire time, turn time, speed-(up/down) time. */
   int			tfire_t, pfire_t, turn_t, speed_t;
   int 			tfire_dir, pfire_dir;
   int			bdc;
   int			bp1, bp2, bp3, bp4;
   double		borg_probability;

   int			sent_distress, lastweap;

} Player;
   
typedef struct _planetrec {
   
   int			total_tarmies, total_textra_armies;
   int			total_warmies, total_wextra_armies;

   int			num_teamsp;
   struct planet	*team_planets[MAXPLANETS];

   int			num_safesp;
   struct planet	*safe_planets[MAXPLANETS];

   int			num_agrisp;
   struct planet	*agri_planets[MAXPLANETS];

   int			num_indsp;
   struct planet	*ind_planets[MAXPLANETS];

   int			num_unknownsp;
   struct planet	*unknown_planets[MAXPLANETS];

   int			num_warteamsp;
   struct planet	*warteam_planets[MAXPLANETS];

} PlanetRec;

#define PL_DAMAGE(e)	((100 * e->damage)/e->p->p_ship.s_maxdamage)
#define PL_FUEL(e)	((100 * e->fuel)/e->p->p_ship.s_maxfuel)
#define PL_SHIELD(e)	((100 * e->shield)/e->p->p_ship.s_maxshield)
#define PL_WTEMP(e)	((100 * e->wpntemp)/e->p->p_ship.s_maxwpntemp)

#define PHRANGE(e)	(PHASEDIST * e->p->p_ship.s_phaserdamage/100)
#define TRRANGE(j)	(int)(((double) TRACTDIST) * (j)->p_ship.s_tractrng)


extern Player		_players[MAXPLAYER];
extern PlanetRec	_planets;

extern Player	       *get_nearest_to_pl();
extern Player	       *get_nearest_to_p();

typedef struct {
   
   int		update_hplanets_t;
   int		update_enemies_t;
   int		robot_attack, pause_ra;
   /* course change due to enemy proximity */
   int		ccep;
   int		disengage_time;
   int		tractor_time, tractor_limit;
   int		bomb_time, take_time;
} Timers;

extern Timers _timers;

#define TRACTOR_TIME	30

typedef struct {
   
   unsigned char	torp_dir[2*MAXTORP];
   int			ntorps;
   int			ipos, opos;
} TorpQ;

extern TorpQ		_torpq;

/* update hostile fly-over planets every 2 seconds */
#define UPDATE_HPLANETS_T	2000
#define UPDATE_ENEMIES_T	2000

#define SH_TORPFUSE(s)		((s)->p_ship.s_torpfuse)
#define SH_TORPCOST(s)		((s)->p_ship.s_torpcost)
#define SH_PLASMACOST(s)	((s)->p_ship.s_plasmacost)
#define SH_PLASMADAMAGE(s)	((s)->p_ship.s_plasmadamage)
#define SH_PLASMASPEED(s)	((s)->p_ship.s_plasmaspeed)
#define SH_PLTURNS(s)		((s)->p_ship.s_plasmaturns)
#define SH_PHASERCOST(s)	((s)->p_ship.s_phasercost)
#define SH_TORPDAMAGE(s)	((s)->p_ship.s_torpdamage)
#define SH_PHASERDAMAGE(s)	((s)->p_ship.s_phaserdamage)
#define SH_ACCINT(s)		((s)->p_ship.s_accint)
#define SH_DECINT(s)		((s)->p_ship.s_decint)
#define SH_TURNS(s)		((s)->p_ship.s_turns)
#define SH_REPAIR(s)		((s)->p_ship.s_repair)
#define SH_RECHARGE(s)		((s)->p_ship.s_recharge)
#define SH_WARPCOST(s)		((s)->p_ship.s_warpcost)
#define SH_CLOAKCOST(s)		((s)->p_ship.s_cloakcost)
#define SH_TRACTRNG(s)		((s)->p_ship.s_tractrng)	/* float */
#define SH_TRACTSTR(s)		((s)->p_ship.s_tractstr)
#define SH_MASS(s)		((s)->p_ship.s_mass)
#define SH_COOLRATE(s)		((s)->p_ship.s_wpncoolrate)

#define TRACTRANGE(p)		(int)(((double)TRACTDIST)*p->p_ship.s_tractrng)

#define NORMALIZE(d)            (((d) + 256) % 256)
#define LOOKAHEAD_I             100
#define MAXFUSE                 100      /* battleship during chaos mode */

#define TORP_INTERVAL(l)        (mtime()-_state.lasttorpreq >= l)
#define PHASER_INTERVAL(l)      (_udcounter -_state.lastphaserreq >= l)
#define DET_INTERVAL(l)		(_udcounter - _state.lastdetreq >= l)

#define MYFUEL()		((100*me->p_fuel)/me->p_ship.s_maxfuel)
#define MYDAMAGE()		((100*me->p_damage)/me->p_ship.s_maxdamage)
#define MYSHIELD()		((100*me->p_shield)/me->p_ship.s_maxshield)
#define MYWTEMP()		((100*me->p_wtemp)/me->p_ship.s_maxwpntemp)
#define MYETEMP()		((100*me->p_etemp)/me->p_ship.s_maxegntemp)

#define ENEMY                   1
#define WAR(j)			((((j)->p_swar | (j)->p_hostile)&me->p_team)\
				 ||((me->p_swar | me->p_hostile)&(j)->p_team))
	

#define MIN_TORP_I                      100	/* ms */
#define MIN_PHASER_I                    6	/* cycles */
#define MIN_DET_I			1	/* cycles */
#define MIN_LOCK_I                      2000	/* unused */

/*
 * State info.  The mind of the robot.
 */

typedef enum {
        S_NOTHING,
	S_COMEHERE,
        S_UPDATE,
        S_ENGAGE,
        S_DEFENSE,
        S_DISENGAGE,
        S_RECHARGE,
        S_NO_HOSTILE,
	S_REFIT,
	S_ASSAULT,
	S_GETARMIES,
	S_OGG,
	S_ESCORT,
        S_QUIT,
} estate;

typedef enum {
        C_NONE,
        C_STAT,
        C_RESET,
	C_COMEHERE,
	C_REFIT,
        C_QUIT,
} ecommand;

typedef enum {
	ENONE,
	ERUNNING,
	EDAMAGE,
	EKILLED,
	ELOWFUEL,
	EPROTECT,
	EREFIT,
	EOVERHEAT,
} ediswhy;

typedef enum {
	EOGGLEFT,
	EOGGRIGHT,
	EOGGTOP,
} eoggtype;

extern int		_battle[MAXPLAYER];

typedef struct {
   int                  status;
   estate               state;
   int                  controller;     /* 1 more than actual player */

   bool                 disengage, closing, do_lock;
   bool			refit, refit_req;
   ediswhy		diswhy;
   bool                 recharge;
   /* protect planet */
   bool			protect, arrived_at_planet;
   struct planet	*protect_planet;

   /* protect player */
   bool			defend, arrived_at_player;
   Player		*last_defend;
   Player		*protect_player;

   /* assault planet */
   bool			assault, assault_req;
   struct planet	*assault_planet;

   /* escort */
   int			escort_req, escort;
   Player		*escort_player;
   struct planet	*escort_planet;

   /* get armies */
   struct planet	*army_planet;

   Player               *closest_e, *current_target;
   int			ogg, ogg_req, ogg_pno, ogg_speed;
   int 			ogg_early_crs;
   eoggtype		ogg_type;
   Player		*closest_f;
   struct planet	*planet;

   ecommand             command;
   int                  lasttorpreq, lastphaserreq, lastdetreq;

   int                  torp_i,
			torp_attack,
			torp_attack_timer;

   Player               *players;
   PlanetRec		*planets;
   short		total_enemies, total_wenemies, total_friends;
   int                  debug;

   /* cheapskate server */
   unsigned char        p_desdir;
   int                  p_desspeed;
   int			p_subdir;
   int			p_subspeed;
   /* TMP -- from server */
   int			sp_subdir;
   int			sp_subspeed;

   /* after death */
   int                  team, ship;

   /* am I alive? */
   bool			dead;

   /* pressor override -- for cases where we are about to blow */
   bool			pressor_override;

   TorpQ		*torpq;

   bool			torp_danger, pl_danger, recharge_danger;
   bool			ptorp_danger;
   int			tdanger_dist, assault_dist, ogg_dist;
   int			assault_range;
   int			det_torps, det_damage, det_const;

   int			*battle;		/* time of last battle */
   bool			chase;

   int			lookahead;

   /* information about last cycle */
   int			prev_speed;
   unsigned char	prev_dir;
   float		timer_delay_ms;		/* for now always 1 */

   /* hostile planet */
   struct planet	*hplanet;
   int			hpldist;

   int			maxfuse, maxspeed;

   char			ignore_e[MAXPLAYER+1];
   float		kills;
   int			killer;

   int			warteam;

   int			lock;

   /* previous cycle information */
   int			last_x, last_y, last_speed, last_desspeed;
   int			last_subspeed, last_subdir;
   unsigned char	last_dir, last_desdir;

   bool			no_assault;

   bool			vector_torps, chaos, wrap_around, torp_wobble;

   bool			beztest, no_speed_given, no_weapon, torp_seek;
   bool			galaxy;
   bool			torp_bounce;
   bool			have_plasma;
   bool			ignore_sudden;
   bool			override;
   bool			vquiet;
   bool			itourn;
   bool			human;
   bool			borg_detect;
   bool			try_udp;
   bool			do_reserved;
   bool			take_notify;

   int			lifetime, seek_const;

   unsigned char	newdir;
   struct _player	*attacking_newdir[MAXPLAYER];

   int			num_tbombers, num_ttakers;

   int			player_type;

   /* difference in number of attackers -vs- team defenders in certain
      radius of space -- update_player_density */
   int			attack_diff;
   int			attackers, defenders;
   int			attack_diff_dist;

} State;

extern State _state;

/* player_type */
#define PT_NORMAL		0
#define PT_OGGER		1
#define PT_CAUTIOUS		2
#define PT_RUNNERSCUM		3
#define PT_DEFENDER		4
#define PT_DOGFIGHTER		5

/* XXX */
extern int	Xplhit;

#define DEFAULT_DISTFUNC	25

#define DEBUG                   _state.debug

#define DEBUG_STATE             0x0001
#define DEBUG_DISENGAGE         0x0002
#define DEBUG_COURSE            0x0004
#define DEBUG_HITS              0x0008
#define DEBUG_TIME		0x0010
#define DEBUG_SERVER		0x0020
#define DEBUG_ENEMY		0x0040
#define DEBUG_PROTECT		0x0080
#define DEBUG_ENGAGE		0x0100
#define DEBUG_WARFARE		0x0200
#define DEBUG_ASSAULT		0x0400
#define DEBUG_RESPONSETIME	0x0800
#define DEBUG_OGG		0x1000
#define DEBUG_ESCORT		0x2000
#define DEBUG_SHMEM		0x4000

#define CFAST			1
#define CSLOW			2

#define WPHASER			1
#define WTORP			2

#define TRECORDED		1

#define SECONDS(x, s)		(_udcounter - x <= s*10) 

#define SHIP(p)			(p->p->p_ship.s_type)

#define ASSAULT_BOMB		2
#define ASSAULT_TAKE		1

#define SQ(x)			(x*x)

#define TR_ESCORT_PRESSOR	1
#define TR_ENEMY_TRACTOR	2
#define TR_EXPLODE		3
#define TR_ORBIT		4
#define TR_DAMAGED		5
#define TR_NOFUEL		6
#define TR_DEFEND		7
#define TR_OGG			8
#define TR_TORP			9

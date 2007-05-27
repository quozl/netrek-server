/* 	$Id: sysdefaults.h,v 1.3 2006/04/26 09:52:43 quozl Exp $	 */

/* structure for default values that are represented as array of flags */
struct sysdef_array {
  char *name;
  int max;
  char **keys;
  int *p;
};

struct sysdef_array sysdefships   =
	{ "SHIPS",   NUM_TYPES,  shiptypes,   shipsallowed   };

struct sysdef_array sysdefweapons =
	{ "WEAPONS", WP_MAX,     weapontypes, weaponsallowed };

struct sysdef_array sysdefplanets =
	{ "PLANETS", MAXPLANETS, planettypes, startplanets   };

/* default value types */
#define SYSDEF_INT	0	/* a simple integer value	*/
#define SYSDEF_FLOAT	1	/* a simple float value		*/
#define SYSDEF_CHAR	2	/* a string value		*/
#define SYSDEF_ARRAY	3	/* an array of flags to be set	*/
#define SYSDEF_ROBOT	4	/* manager_type is to be set	*/
#define SYSDEF_SHIP	5	/* shipvals is to be called	*/
#define SYSDEF_NOTHING	6	/* unimplemented type, ignore	*/
#define SYSDEF_END	-1	/* marks end of keyword table	*/

struct sysdef_keywords {
  char *key;			/* keyword to match		*/
  int type;			/* type of keyword as per above	*/
  void *p;			/* pointer to variable		*/
/*
	SYSDEF_INT	(int *)
	SYSDEF_CHAR	(char *)
	SYSDEF_ARRAY	(struct sysdef_array *)
	SYSDEF_ROBOT	(const char *)
	SYSDEF_SHIP	NULL
	SYSDEF_END	NULL
*/
  char *text;			/* explanatory text for key	*/
} sysdef_keywords[] =
{
  { "ADMIN_PASSWORD",	SYSDEF_CHAR,	admin_password,
    "Admin password" },
  { "CLUE",		SYSDEF_INT,	&clue,
    "Message based clue checking"			},
  { "CLUERANK",		SYSDEF_INT,	&cluerank,
    "No clue checking from this rank on"		},
  { "HUNTERKILLER",	SYSDEF_INT,	&killer,
    "Send in periodic killer robot"			},
  { "RESETGALAXY",	SYSDEF_INT,	&resetgalaxy,
    "Reset galaxy on daemon start"			},
#ifdef CHECKMESG
  { "CHECKMESG",	SYSDEF_INT,	&checkmessage,
    "Check messages for configuration"			},
  /* CHECKMESG seems to do not much at all, internally. */

  { "LOGALL",		SYSDEF_INT,	&logall,
    "Log all messages to file"				},
  { "LOGGOD",		SYSDEF_INT,	&loggod,
    "Log messages to GOD in a separate file"		},
  { "EVENTLOG",		SYSDEF_INT,	&eventlog,
    "Record warnings in log for post-game parsing"	},
#endif /* CHECKMESG */
  { "DOOSHER",		SYSDEF_INT,	&doosher,
    "Invent a message when armies die in flight"	},
  { "CHECKSCUM",	SYSDEF_INT,	&check_scum,
    "Check for multiple clients from same IP address"	},
  { "IP_CHECK_DNS",	SYSDEF_INT,	&ip_check_dns,
    "Reverse/forward lookup on IP addresses"		},
  { "IP_CHECK_DNS_VERBOSE",	SYSDEF_INT,	&ip_check_dns_verbose,
    "Message to ALL board on IP/DNS mismatch"		},
  { "WHYMESS",		SYSDEF_INT,	&whymess,
    "When a ship explodes, indicate why"		},
  { "MAX_POP",		SYSDEF_INT,	&max_pop,
    "Maximum army population on a planet outside T-Mode" },
  { "SHOWSYSDEF",	SYSDEF_INT,	&show_sysdef,
    "Show .sysdef contents on client on login"		},
  { "PLANET_MOVE",	SYSDEF_INT,	&planet_move,
    "Move planets in orbits around primary"		},
  { "WRAP_GALAXY",	SYSDEF_INT,	&wrap_galaxy,
    "Wrap galactic borders"				},
#ifdef MINES
  { "STARBASE_MINES",	SYSDEF_INT,	&mines,
    "Permit starbase mines (unimplemented)"		},
#endif
  { "PINGPONG_PLASMA",	SYSDEF_INT,	&pingpong_plasma,
    "Plasma can bounce if phasered"			},
  { "MAX_CHAOS_BASES",	SYSDEF_INT,	&max_chaos_bases,
    "Maximum number of bases in CHAOS mode"		},
  { "TOURN",		SYSDEF_INT,	&tournplayers,
    "Number of players per side for T-Mode"		},
  { "SAFE_IDLE",	SYSDEF_INT,	&safe_idle,
    "Allow cloakers on homeworld to idle safely if no t-mode" },
  { "SHIPS",		SYSDEF_ARRAY,	&sysdefships,
    "Ships that may be flown"				},
  { "WEAPONS",		SYSDEF_ARRAY,	&sysdefweapons,
    "Weapons that may be used"				},
  { "PLKILLS",		SYSDEF_INT,	&plkills,
    "Kills required before plasma granted on refit"	},
  { "DDRANK",		SYSDEF_INT,	&ddrank,
    "Rank required to refit to a Destroyer"		},
  { "GARANK",		SYSDEF_INT,	&garank,
    "Rank required to refit to a Galaxy class ship"	},
  { "SBRANK",		SYSDEF_INT,	&sbrank,
    "Rank required to refit to Starbase"	 	},
  { "SBPLANETS",	SYSDEF_INT,	&sbplanets,
    "Minimum planets for Starbase"			},
  { "SBORBIT",          SYSDEF_INT,	&sborbit,
    "Allow bases to orbit enemy planets"		},
  { "CHAOS",		SYSDEF_INT,	&chaosmode,
    "Enable CHAOS mode" 				},
#ifdef SNAKEPATROL
  { "SNAKEPATROL",	SYSDEF_INT,	&snakepatrol,
    "Strange snake torpedo stream (unimplemented)"	},
#endif
  { "NODIAG",		SYSDEF_INT,	&nodiag,
    "Prohibit diagonal games (e.g. ROM vs ORI)"		},
  { "CLASSICTOURN",	SYSDEF_INT,	&oldtourn,
    "Use classic tournament mask mode"			},
  { "REPORT_USERS",	SYSDEF_INT,	&reportmetausers,
    "Report users on metaservers"			},
  { "TOPGUN",		SYSDEF_INT,	&topgun,
    "Enable TOPGUN mode"				},
  { "NEWTURN",		SYSDEF_INT,	&newturn,
    "Enable revised turning circle rules"		},
  { "HIDDEN",		SYSDEF_INT,	&hiddenenemy,
    "Hide distant players in T-Mode"			},
  { "LOADCHECK",	SYSDEF_INT,	&loadcheck,
    "Check system load via uptime"			},
  { "MAXLOAD",		SYSDEF_FLOAT,	&maxload,
    "Close game if system load exceeds"			},
  { "PLANETS",		SYSDEF_ARRAY,	&sysdefplanets,
    "Starting planets for each race"			},
  { "CONFIRM",		SYSDEF_INT,	&binconfirm,
    "Perform client program verification"		},
  { "UDP",		SYSDEF_INT,	&udpAllowed,
    "Use UDP communications for speed"			},
  { "SURRSTART",	SYSDEF_INT,	&surrenderStart,
    "Number of planets at which surrender starts"	},
#ifdef PING
  { "PING_FREQ",	SYSDEF_INT,	&ping_freq,
    "Frequency of ping packets to clients"		},
  { "PING_ILOSS_INTERVAL",SYSDEF_INT,	&ping_iloss_interval,
    "Incremental packet loss sampling interval"		},
  { "PING_GHOSTBUST",	SYSDEF_INT,	&ping_allow_ghostbust,
    "Ghostbust ships if we lose pings to client"	},
  { "PING_GHOSTBUST_INTERVAL", SYSDEF_INT, &ping_ghostbust_interval,
    "Interval without pings before ghostbust"		},
#endif /* PING */
  { "TRANSWARP",	SYSDEF_INT,	&twarpMode,
    "Enable transwarp to Starbase" },
  { "WARPSPEED",	SYSDEF_INT,	&twarpSpeed,
    "Transwarp equivalent warp speed" },
  { "VECTOR",		SYSDEF_INT,	&vectortorps,
    "Vector torps (unimplemented)" },
  { "START_ARMIES",	SYSDEF_INT,	&top_armies,
    "Starting planet army count" },
  { "ERROR_LEVEL",	SYSDEF_INT,	&errorlevel,
    "Server error log verbosity level" },
#ifdef BASEPRACTICE
  { "BASEPRACTICE",	SYSDEF_ROBOT,	(void *) BASEP_ROBOT,
    "Enable base practice robot on startup" },
#endif
#ifdef NEWBIESERVER
  { "NEWBIE",	SYSDEF_ROBOT,	(void *) NEWBIE_ROBOT,
    "Enable newbie server robot on startup" },
  { "MIN_NEWBIE_SLOTS",     SYSDEF_INT,     &min_newbie_slots,
    "Minimum number of total (human + robot) players to maintain" },
  { "MAX_NEWBIE_PLAYERS",     SYSDEF_INT,     &max_newbie_players,
    "Maximum number of human players to allow at once" },
  { "NEWBIE_BALANCE_HUMANS",	SYSDEF_INT,	&newbie_balance_humans,
    "Keep number of humans on each team balanced" },
#endif
#ifdef PRETSERVER
  { "PRET",	SYSDEF_ROBOT,	(void *) PRET_ROBOT,
    "Enable pre T entertainment robot on startup" },
  { "PRET_GUEST",	SYSDEF_INT,	&pret_guest,
    "Use guest logins instead of random logins" },
#endif
#if defined(BASEPRACTICE) || defined(NEWBIESERVER)  || defined(PRETSERVER)
  { "ROBOTHOST",	SYSDEF_CHAR,	robot_host,
    "Robot host" },
#endif
  { "HOCKEY",		SYSDEF_ROBOT,	(void *) PUCK_ROBOT,
    "Enable Puck (hockey) robot on startup" },
  { "GALACTIC_SMOOTH",	SYSDEF_INT,	&galactic_smooth,
    "Cause galactic to be updated at same rate as tactical" },
  { "INL",		SYSDEF_ROBOT,	(void *) INL_ROBOT,
    "Enable INL (league) robot on startup" },
  { "INL_RECORD",	SYSDEF_INT,	&inl_record,
    "Record INL games server side with CamBot" },
#ifdef DOGFIGHT
  { "DOGFIGHT",		SYSDEF_ROBOT,	(void *) MARS_ROBOT,
    "Enable Mars (dogfight) robot on startup" },
  { "CONTESTSIZE",	SYSDEF_INT,	&contestsize,
    "Size of dogfight practice contest" },
  { "NUMMATCH",		SYSDEF_INT,	&nummatch,
    "Number of dogfight practice matches" },
  { "SAVE_DOG_STAT",	SYSDEF_INT,	&dogcounts,
    "Save dogfight practice statistics" },
#endif /* DOGFIGHT */
  { "MOTD",		SYSDEF_CHAR,	Motd,
    "Message of the day" 	},
  { "SHIP",		SYSDEF_SHIP,	NULL,
    "Ship characteristics" 	},
  { "REALITY",		SYSDEF_INT,	&distortion,
    "Time distortion constant" },
  { "FPS",		SYSDEF_INT,	&fps,
    "Frames per second (10 from 1986, 50 from 2007)" },
  { "MAXUPS",		SYSDEF_INT,	&maxups,
    "Maximum client updates per second" },
  { "DEFUPS",		SYSDEF_INT,	&defups,
    "Default client updates per second" },
  { "MINUPS",		SYSDEF_INT,	&maxups,
    "Minimum client updates per second" },
  { "RESTRICT_3RD_DROP",	SYSDEF_INT,	&restrict_3rd_drop,
    "No 3rd space dropping"				},
  { "RESTRICT_BOMB",	SYSDEF_INT,	&restrict_bomb,
    "No bombing out of t-mode"				},
  { "NO_UNWARRING_BOMBING",	SYSDEF_INT,	&no_unwarring_bombing,
    "No 3rd space bombing"				},
  { "OBSERVER_MUTING",   SYSDEF_INT,     &observer_muting,
    "Turn off messaging for observers"			},
  { "OBSERVER_KEEPS_GAME_ALIVE",   SYSDEF_INT,     &observer_keeps_game_alive,
    "Whether an observer slot is counted for keeping game alive" },
  { "VOTING",		SYSDEF_INT,	&voting,
    "Enable voting"			},
  { "BAN_VOTE_ENABLE",	SYSDEF_INT,	&ban_vote_enable,
    "Enable vote for temporary ban"			},
  { "BAN_VOTE_DURATION",	SYSDEF_INT,	&ban_vote_duration,
    "Length of ban in seconds"	},
  { "BAN_NOCONNECT",		SYSDEF_INT,	&ban_noconnect,
    "Deny permanently banned players a slot"	},
  { "EJECT_VOTE_ENABLE",	SYSDEF_INT,	&eject_vote_enable,
    "Enable vote for ejection"				},
  { "EJECT_VOTE_ONLY_IF_QUEUE",	SYSDEF_INT,	&eject_vote_only_if_queue,
    "Only allow ejection if there is a queue of players"	},
  { "EJECT_VOTE_VICIOUS",	SYSDEF_INT,	&eject_vote_vicious,
    "Kill ntserv process to eject, causes some client problems"	},
  { "DUPLICATES",		SYSDEF_INT,	&duplicates,
    "Maximum number of duplicate connections from a single IP" },
  { "BLOGGING",		SYSDEF_INT,	&blogging,
    "Enable blogging of server events" },
#ifdef STURGEON
  { "STURGEON",			SYSDEF_INT,	&sturgeon,
    "Use kills to buy ship upgrades" },
  { "STURGEON_SPECIAL_WEAPONS",	SYSDEF_INT,	&sturgeon_special_weapons,
    "Allow special type weapons like nukes, drones, and mines" },
  { "STURGEON_MAXUPGRADES",	SYSDEF_INT,	&sturgeon_maxupgrades,
    "# of kills worth of upgrades that can be purchased" },
  { "STURGEON_EXTRAKILLS",	SYSDEF_INT,	&sturgeon_extrakills,
    "Get .15 kill credit per upgrade on ships that are killed" },
  { "STURGEON_PLANETUPGRADES",	SYSDEF_INT,	&sturgeon_planetupgrades,
    "Get free upgrades for taking planets in enemy space" },
  { "STURGEON_LITE",		SYSDEF_INT,	&sturgeon_lite,
    "Only lose most expensive upgrade on death." },
#endif
  { "STARBASE_REBUILD_TIME",	SYSDEF_INT,	&starbase_rebuild_time,
    "Rebuild time of starbase, in minutes." },
  { "",			SYSDEF_END,	NULL		}
};

/*

Procedure for adding new default value.
James Cameron, 14th February 2004.

- add to ntserv/data.c
- add to include/data.h
- add to sysdef_keywords in include/sysdefaults.h, in any order, prefer end
- add to docs/sample_sysdef.in with comment about value meanings
- use glade (http://glade.pn.org/) to add to gum/gum.xml and regenerate code
- leave a comment below if your defaults have not been added to gum

STARBASE_REBUILD_TIME
STURGEON*
EJECT_VOTE_*
BAN_VOTE_*
DUPLICATES
MAX_NEWBIE_*
NEWBIE_BALANCE_HUMANS
SAFE_IDLE
OBSERVER_MUTING
BAN_NOCONNECT
ADMIN_PASSWORD
DDRANK
GARANK
BLOGGING
FPS
SBORBIT
OBSERVER_KEEPS_GAME_ALIVE
RESTRICT_3RD_DROP
IP_CHECK_DNS*
CLASSICTOURN
REPORT_USERS

*/

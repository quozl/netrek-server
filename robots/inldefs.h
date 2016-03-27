/* basepdefs.h

   This file contains many of the defines needed for Base practice
   and not for anything else.

*/
#ifndef _h_inldefs
#define _h_inldefs


#include "defs.h"

/*
#define INLDEBUG	1
*/


/* Fuses. These are used to control the timing of events */

/* Constant values must be in () or the constants might be misevaluated
   depending on the syntax it is used in */

#define PLAYERFUSE	1
#define PLANETFUSE	(400/MAXPLANETS)	/* INL 3.9 POP */
#define INLSTARTFUSE	(60*PERSEC)	/* 1 minute fuse */
#define TIMEFUSE	PERSEC


/* System dependent setups */

#define PERSEC		fps		/* Number of checks per second */
#define PERMIN		(60*PERSEC)	/* Number of checks per minute */

#define HOMETEAM	FED		/* Entry point for home team */
#define AWAYTEAM	ROM		/* Entry point for away team */
#define TEAMSIZE	8		/* Number of players per team */
#define NUMTEAMS	4		/* Number of sides */

#define HOME		0
#define AWAY		1
#define INLTEAM		AWAY+1		/* probably shouldn't change this */
					/* why? because we're assuming that */
					/* the hometeam is 0 and the away */
					/* is 1 */
#define N_INLLOG	"./INL_log"

#define C_PR_CAPTAIN    C_PR_1
#define C_PR_PREGAME    C_PR_2
#define C_PR_INGAME     C_PR_3

/* TEAM flags */

#define T_START		0x01
#define T_RESETGALAXY	0x02
#define T_PAUSE		0x04
#define T_CONTINUE	0x08
#define T_RESTART	0x10
#define T_TIMEOUT	0x20
#define T_SIDELOCKED    0x40            /* Pregame only-side cant choose */ 
#define T_CONFINE       0x80            /* Pregame only */
#define T_REGISTER      0x100           /* Captain requests registration */
#define T_DRAFT         0x200

/* INL_STAT flags */

#define S_TMOUT		0x01		/* Someone has called a timeout */
#define	S_COUNTER	0x02		/* This is the 10 second countdown */
#define S_FREEZE	0x04		/* Everyone is suspended (ie: pause) */
#define	S_OVERTIME	0x08		/* Game is in overtime! */
#define S_PREGAME	0x10		/* pregame chaos mode */
#define	S_OFFICIAL	0x20		/* Offical game */
#define S_TOURNEY	0x40		/* tournament under way! */
#define S_COUNTDOWN	0x80		/* 1 minute until start countdown */
#define S_CONFINE       0x100           /* Confine players */
#define S_DRAFT		0x200		/* Draft is underway */

#define NONE		-1

#define NOT_CHOOSEN	-1
#define RELINQUISH	-2		/* Relinquish choosing team first */

typedef struct _team {
	short	team;			/* Away or home team */
	char	*name;			/* away or home string */
	char	*t_name;		/* Name of team */
	int	captain;		/* who is captain */
	short	side;			/* Race flag */
	int	side_index;		/* index into side array */
	int	start_armies;		/* armies requested to start with */
	int	time;			/* requested length of game */
	int	overtime;		/* reguested length of overtime */
	int	flags;			/* team flags */
	int 	tmout;			/* tmout counter */
	int	planets;		/* number of planets owned */
					/* see README.scores for details
					   on continuous scoring.  -da */
	char	score_mode;		/* scoring mode */
	int	abs_score;		/* absolute continuous score */
	int	semi_score;		/* semi-continuous score */
	double	weighted_score;		/* weighted continuous score */
} Team;

typedef struct _inl_stat {
	int	start_armies;		/* offical starting armies */
	int	change;			/* 1 if things can be changed */
	int	flags;
	int	ticks;                  /* daemon ticks */
	int     game_ticks;             /* ticks in official game */
	int     tmout_ticks;
	int	time, overtime;
        int	extratime;
	int	remaining;              /* remaining time for display only */
	int	score_mode;
	double	weighted_divisor;
	int swap[2];			/* two slots to be traded */
	int swapnum;			/* number of slots to trade in pending
	                                   trade request */
	int swapteam;			/* team requesting current trade */
} Inl_stats;


typedef struct  _sides {
	char	*name;
	int	flag;
	int	diagonal;
} Sides;

typedef struct _inl_countdown {
	int	idx;		/* Current status */
	int	counts[20];	/* Message interval */
	int 	act;		/* Index for next action */
	int	end;		/* null time for countdown */
	int	(*action) ();	/* Function to execute */
	char	*message;	/* Message to print */
} Inl_countdown;

void start_countdown();
void reset_inl(int);

#endif /* _h_inldefs */

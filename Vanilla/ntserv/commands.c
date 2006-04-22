/*
 *  Commands.c		
 */

/*
	Generic command interface for netrek. This interface can be used for
	Puck, Dog, and normal server. The defines DOG and PUCK are used to
	distinguish the different possible commands allowed. If neither one
	is defined then it is a normal server command.
*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "ltd_stats.h"
#include "proto.h"
#ifdef DOG
#include "marsdefs.h"
#endif
#ifdef PUCK
#include "puckdefs.h"
#endif
#ifdef BASEP
#include "basepdefs.h"
#endif
#ifdef INL
#include "inldefs.h"
#endif

/***** Command Interface Defines *****/
/*     The lowest byte is used for message flags. This is used mostly by the */
/*     voting code. */


#define C_PLAYER	0x10
#define C_TEAM		0x20
#define C_CAPTAIN	0x40
#define C_NAME		0x80


/***** Local Functions *****/

int do_help();
char *check_possible(char *line, char *command);
int getplayer(int from, char *line);

#ifdef VOTING
int do_generic_vote(char *comm, int who);
int do_start_robot();
#endif

#if defined (ALLOW_EJECT) && !defined(INL)
int do_player_eject();
#endif

#if defined (AUTO_INL) && !defined(INL)
int do_start_inl();
#endif

#if defined (AUTO_PRACTICE) && !defined(BASEP)
int do_start_basep();
#endif

#if defined(AUTO_HOCKEY) && !defined(PUCK)
int do_start_puck();
#endif

#if defined(AUTO_DOGFIGHT) && !defined(DOG)
int do_start_mars();
#endif

#if defined (TRIPLE_PLANET_MAYHEM)
char *teamNames[9] = {" ", "Federation", "Romulans", " ", "Klingons",
                      " ", " ", " ", "Orions"};
int do_balance();
int do_triple_planet_mayhem();
#endif

#if defined (DOG) || defined (PUCK)	/* DOG-FIGHTING & PUCK stuff */

extern int do_score_msg();
extern int do_stats_msg();
extern int do_version();
extern int do_force_newgame();
extern int do_sitout();

#endif

#ifdef DOG				/* DOG-FIGHTING stuff */

char myname[] = {"Mars"};

extern int do_status_msg();
extern int do_join();
extern int do_preferred_ship();

#elif defined (PUCK)			/* PUCK stuff */

char myname[] = {"Puck"};

extern int do_change_timekeep();
#ifdef ALLOW_PAUSE
extern int game_pause();
extern int game_resume();
#endif

#elif defined (INL)			/* INL stuff */

char myname[] = {"INL"};

extern int do_home();
extern int do_away();
extern int do_start();
extern int do_captain();
extern int do_uncaptain();
extern int do_tname();
extern int do_pickside();
extern int do_resetgalaxy();
extern int do_switchside();
extern int do_pause();
extern int do_free();
extern int do_gametime();
extern int do_timeout();
extern int do_confine();

#else					/* OTHER stuff */


#if defined (BASEP) && defined(BASEPRACTICE)	/* BASEP stuff */

char myname[] = {"Smack"};

int do_sbstats_query();
extern int do_start_robot();

#else		 			/* Other section's functions */

char myname[] = {"GOD"};

int do_sbstats_query();
int do_time_msg();
int do_queue_msg();

#ifdef GENO_COUNT
int do_genos_query();
#endif


#endif
#endif

#ifndef INL
int do_client_query();
int do_ping_query();
int do_stats_query();
int do_whois_query();
#endif

#if (defined(AUTO_INL) && defined(INL)) || (defined(BASEP) && defined(AUTO_PRACTICE)) || (defined(AUTO_HOCKEY) && defined(PUCK)) || (defined(AUTO_DOGFIGHT) && defined (DOG))
extern int cleanup();
#endif

/********* COMMANDS LIST ********
	Note: - The commands *must* be in upper case. All commands are converted
		to upper case before be checked. 
	      - Votes are entered in the Voting list.
	The first field is the command, the second is a description string 
		(up to 50 chars.), lastly is the function to call.
*/

struct command_handler commands[] = {
    { NULL, 0, NULL, NULL},         /* record 0 */
    { "HELP",
		0,
		"Show this help information",
		do_help },     			/* HELP */
#ifndef INL
    { "CLIENT",
		C_PLAYER,
		"Show a user's client. Ex: 'CLIENT 0'",
		do_client_query },		/* CLIENT */
    { "PING",
		C_PLAYER,
		"Show client's ping (lag). Ex: 'PING 0'",
		do_ping_query },		/* PING */
    { "STATS",
		C_PLAYER,
		"Show player's stats during t-mode. Ex: 'STATS 0'",
		do_stats_query },		/* STATS */
    { "WHOIS",
		C_PLAYER,
		"Show a player's login info. Ex: 'WHOIS 0'",
		do_whois_query },		/* WHOIS */
#endif

#if defined (DOG) || defined(PUCK)		/***** DOG & PUCK commands */
    { "SCORE",
		0,
		"Show the current standings",
		do_score_msg },			/* SCORE */	
    { "PSTATS",
		0,
		"Show your current stats",
		do_stats_msg },			/* local STATS */
    { "VERSION",
		0,
#ifdef DOG
                "Show Mars' version",
#else
                "Show Puck's version",
#endif
		do_version },			/* VERSION */
    { "SITOUT",
		0,
#ifdef DOG
		"Sitout from the competition",
#else
		"You will be moved off the rink.",
#endif
		do_sitout },			/* SITOUT */
#endif	/* DOG or PUCK */
#ifdef DOG					/***** DOG only commands */
    { "STATUS",
		0,
		"Show the mode Mars is in",
		do_status_msg },		/* STATUS */
    { "JOIN",
		0,
		"Join the competition",
		do_join },			/* JOIN */
    { "WATCH",
		0,
		"Sitout from the competition",
		do_sitout },			/* WATCH */
    { "SHIP",
		0,
		"Select your preferred ship. Ex: 'SHIP CA'",
		do_preferred_ship },		/* SHIP */
#elif defined (PUCK)				/***** PUCK only commands */
#ifdef ALLOW_PAUSE
    { "PAUSE",
                0,
                "Pause the game",
                game_pause },
    { "UNPAUSE",
                0,
                "Resume the game",
                game_resume },
#endif
    /* None yet... */

#elif defined (INL)				/***** INL only commands */
#ifdef nodef
    { "HOME",
		0,
		"Switch to home team.",
		do_home },			/* HOME */
    { "AWAY",
		0,
		"Switch to away team.",
		do_away },			/* AWAY */
#endif
    { "HOME",
		0,
		"Switch to home team.",
		do_switchside },		/* HOME */
    { "AWAY",
		0,
		"Switch to away team.",
		do_switchside },		/* AWAY */
    { "START",
		C_CAPTAIN,
		"Start tournament.",
		do_start },			/* START */
    { "CAPTAIN",
		0,
		"Make yourself captain of team.",
		do_captain },			/* CAPTAIN */
    { "UNCAPTAIN",
		C_CAPTAIN,
		"Release captain duties.",
		do_uncaptain },			/* UNCAPTAIN */
    { "TNAME",
		C_CAPTAIN | C_NAME,
		"Change the name of your team.",
		do_tname },			/* TNAME */
    { "RESETGALAXY",
		C_CAPTAIN,
		"Reset the galaxy.",
		do_resetgalaxy },		/* RESETGALAXY */
    { "FED",
		C_CAPTAIN,
		"Change your side to Federation.",
		do_pickside },			/* FED */
    { "ROM",
		C_CAPTAIN,
		"Change your side to Romulan.",
		do_pickside },			/* ROM */
    { "KLI",
		C_CAPTAIN,
		"Change your side to Klingon.",
		do_pickside },			/* KLI */
    { "ORI",
		C_CAPTAIN,
		"Change your side to Orion.",
		do_pickside },			/* ORI */
    { "PASS",
		C_CAPTAIN,
		"Pass the first choice of sides to the home team.",
		do_pickside },			/* PASS */
    { "PAUSE",
		C_CAPTAIN,
		"Requests a pause.",
		do_pause },			/* PAUSE */
    { "PAUSENOW",
		C_CAPTAIN,
		"Imediately pauses the game.",
		do_pause },			/* PAUSENOW */
    { "CONTINUE",
		C_CAPTAIN,
		"Requests that the game continues.",
		do_pause },			/* CONTINUE */
    { "TOUT",
		C_CAPTAIN,
		"Call a timeout.",
		do_timeout },			/* TIMEOUT */
    { "FREE",
		C_CAPTAIN | C_PLAYER,
		"Frees a slot. Ex: 'FREE 0'",
		do_free },			/* FREE */
    { "GAMETIME",
		C_CAPTAIN,
		"Shows the game time or sets it. Ex: 'GAMETIME 30 10'",
		do_gametime },			/* GAMETIME */
    { "CONFINE",
		C_CAPTAIN,
		"Confine teams from other races cores.",
		do_confine },			/* CONFINE */
#else
    { "SBSTATS",
                C_PLAYER,
                "Show player's starbase stats. Ex: 'SBSTATS 0'",
                do_sbstats_query },             /* SBSTATS */

#if defined (BASEP) && defined (BASEPRACTICE)   /***** Basepract. commands */
    { "START",
		0,
		"Start practice robots. Ex: 'START 4 ROM'",
		do_start_robot },

#else
						/***** Vanilla commands */
    { "TIME",
		0,
		"Show time left on Genocide timer.",
		do_time_msg },			/* TIME */
    { "QUEUE",
		0,
		"Show how many people are on the queue.",
		do_queue_msg },			/* QUEUE */
#ifdef GENO_COUNT
    { "GENOS",
		C_PLAYER,
		"Show player's winning genocides.  Ex: 'GENOS 0'",
		do_genos_query },                       /* GENOS */
#endif
#endif
#endif
    };

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]) )




#ifdef VOTING

/*********** VOTING LIST ***********
      Note: The vote list *must* must be in upper case.

      First field is the command name
      Second is whether it is followed by a player number (VC_PLAYER),
          whether it is saved in the godlog (VC_GLOG), whether everyone
          (VC_ALL) or just teammates (VC_TEAM) vote on it.
      Third is the minimum number of votes to pass.
      Fourth is the starting element in the voting list.
      Fifth is a description string (up to 50.)
      Sixth is the how often the vote can be used in seconds (0 for no limit).
      Seventh is the function to call.

        Note:  Undesired results can occur if VC_ALL and VC_TEAM are set.
      Note:  Only defines containing VC_ should be used in votes.
*/

struct vote_handler votes[] = {
    { NULL, 0, 0, 0, NULL, 0, NULL},         			/* record 0 */
#if defined(ALLOW_EJECT) && !defined(INL)
    { "EJECT",  
	VC_TEAM | VC_GLOG | VC_PLAYER,
	2,
	0,
	"To eject a player. Ex: 'EJECT 0'", 
	120,
	do_player_eject },				/* EJECT */
#endif
#if defined(TRIPLE_PLANET_MAYHEM) && !defined(ROBOT)
    { "TRIPLE",
        VC_ALL | VC_GLOG,
        2,
        22,
        "Start triple planet mayhem by vote",
	0,
        do_triple_planet_mayhem },
    { "BALANCE",
        VC_ALL | VC_GLOG,
        4,
        23,
        "Request team randomise & balance",
	0,
        do_balance },
#endif
#if defined (DOG) || defined (PUCK)
  { "NEWGAME", 
	VC_ALL, 
	1,
	20,
	"Game can be restarted by majority vote", 
	0,
	do_force_newgame },				/* NEWGAME */
#endif
#ifdef PUCK
  { "PUCKCLOCK", 
        VC_ALL,
	1, 
	21,
	"Time keeping can be changed by majority vote", 
	0,
	do_change_timekeep },                           /* Short game */
#endif
#if defined(AUTO_INL) && !defined(ROBOT)
  { "INL",
	VC_ALL|VC_GLOG,
	1,
	20,
	"Start game under INL rules.",
	0,
	do_start_inl},
#endif
#if (defined(AUTO_INL) && defined(INL)) || (defined(BASEP) && defined(AUTO_PRACTICE)) || (defined(AUTO_HOCKEY) && defined(PUCK)) || (defined(AUTO_DOGFIGHT) && defined (DOG))
  { "EXIT",
	VC_ALL,
	1,
	20,
	"Terminate robot by majority vote.",
	0,
	cleanup},
#endif
#if defined(AUTO_PRACTICE) && !defined(ROBOT)
  { "PRACTICE",
	VC_ALL,
	1,
	20,
	"Start basepractice by majority vote.",
	0,
	do_start_basep},
#endif
#if defined(AUTO_HOCKEY) && !defined(ROBOT)
  { "HOCKEY",
	VC_ALL|VC_GLOG,
	1,
	20,
	"Start hockey by majority vote.",
	0,
	do_start_puck},
#endif
#if defined(AUTO_DOGFIGHT) && !defined(ROBOT)
  { "DOGFIGHT",
	VC_ALL|VC_GLOG,
	1,
	20,
	"Start dogfight tournament by majority vote.",
	0,
	do_start_mars},
#endif
    };


#define NUM_VOTES (sizeof(votes) / sizeof(votes[0]) )

#endif /* VOTING */


/*	Using just strstr is not enough to ensure uniqueness. There are 
	commands such as STATS and SBSTATS that could be matched in two
	different ways when searching for STATS with strstr.  - nbt */

char *check_possible(char *line, char *command)
{
    int len;
    register int i, j = 0;
    int start = 0;

    len = strlen(line);

    if (len < 8) return NULL;

    for (i = 8; i < len && isspace(line[i]); i++) ;	/* skip header/space */

    if (i >= len) return NULL;				/* empty line */
    start = i;

    for (; i < len; i++) {				/* check command */
	if (command[j] == '\0') break;
	if (command[j++] != line[i])
	    return NULL;
    }

/* +++ 2.6pl2 kantner@hot.caltech.edu */
    if ((i - start) < strlen(command) ) {
        return NULL;
    }
/* --- */

    return (&line[start]);
}


/* Check the command list to see if a message is a command */

int check_command(mess)
    struct message *mess;
{
    int p,i,j;
    char *upper;
    char *comm;
    int who = -1;

    if (!(mess->m_flags & MINDIV)) return 0;	/* double-check */

    upper = strdup(mess->m_data);

    for (p=0; p<strlen(upper); p++) {
	upper[p] = toupper(upper[p]);

        for (i=1; i < NUM_COMMANDS; i++) {
	    if ((comm=check_possible(upper,commands[i].command))!=NULL) {
		if (!(commands[i].tag & C_NAME)) {
		   for (j=i; j < strlen(upper); j++)
			upper[j] = toupper(upper[j]);
		}
		if (commands[i].tag & C_PLAYER) {
		    if ((who = getplayer(mess->m_from,comm)) < 0) return 0;
			(*commands[i].handler)(comm,mess,who);
		} else
			(*commands[i].handler)(comm,mess);
		free (upper);
		return 1;
	    }
        }
    }
    i = 0;
#ifdef VOTING
    i = do_generic_vote(upper,mess->m_from);	/* maybe it's a vote? */
#endif
    free (upper);
    return (i);
}

int getplayer(int from, char *line)
{
    char id;
    char temp[MSG_LEN];
    int what;
    char last;
    int numargs;

    numargs = sscanf(line,"%s %c", temp, &id);

    last = 'A' + (MAXPLAYER - 10);

    if (numargs==1)
	what = from;
    else if ((id >= '0') && (id <='9'))
	what = id - '0';
    else if ((id >= 'A') && (id <= last))
	what = id - 'A' + 10;
    else {
	pmessage(from, MINDIV, addr_mess(from,MINDIV), " Invalid player number.");
	return -1;
    }
    if (players[what].p_status == PFREE) {
	pmessage(from, MINDIV, addr_mess(from,MINDIV), " Player not in the game.");
	return -1;
    }
    return what;
}





#if defined(TRIPLE_PLANET_MAYHEM)
/*
**  16-Jul-1994 James Cameron
**
**  Balances teams according to player statistics.
**  Intended for use with Triple Planet Mayhem
*/

/*
**  Tell all that player will be moved
*/
static void moveallmsg ( int p_no, int ours, int theirs )
{
    struct player *k = &players[p_no];
    pmessage(0, MALL, "GOD->ALL",
       "Balance: %16s (%c%c) is to join the %s",
       k->p_name, teamlet[k->p_team], shipnos[p_no], teamNames[ours] );
}

/*
**  Move a player to the specified team, if they are not yet there.
**  Make them peaceful with the new team, and hostile/at war with the
**  other team.
*/
static void 
move(int p_no, int ours, int theirs)
{
  struct player *k = &players[p_no];
  int queue;
  
  if ( k->p_team != ours ) {
    pmessage(k->p_no, MINDIV, addr_mess(k->p_no,MINDIV),
	     "%s: please SWAP SIDES to the %s", k->p_name, teamNames[ours] );
  }
  else {
    pmessage(k->p_no, MINDIV, addr_mess(k->p_no,MINDIV),
	     "%s: please remain with the %s", k->p_name, teamNames[ours] );
  }
  
  printf("Balance: %16s (%s) is to join the %s\n", 
	 k->p_name, k->p_mapchars, teamNames[ours]);
  
  /* cope with a balance during INL pre-game, if we don't shift players who
     are on the QU_HOME or QU_AWAY queues then the queue masks will force
     them to join the team they were on anyway. */
  queue = ( ours == FED ) ? QU_HOME : QU_AWAY;
  if (k->w_queue != QU_PICKUP && k->w_queue != queue) {
    queues[k->w_queue].free_slots++;
    k->w_queue = queue;
    queues[k->w_queue].free_slots--;
  }  
  
  k->p_hostile |= theirs;
  k->p_swar    |= theirs;
  k->p_hostile &= ~ours;
  k->p_swar    &= ~ours;
  k->p_war      = (k->p_hostile | k->p_swar);
  k->p_team     =  ours;
  sprintf(k->p_mapchars, "%c%c", teamlet[k->p_team], shipnos[p_no]);
  sprintf(k->p_longname, "%s (%s)", k->p_name, k->p_mapchars);
  
  k->p_status = PEXPLODE;
  k->p_whydead = KPROVIDENCE; /* should be KTOURNSTART? */
  if (k->p_ship.s_type == STARBASE)
    k->p_explode = 2 * SBEXPVIEWS;
  else
    k->p_explode = 10;
  k->p_ntorp = 0;
  k->p_nplasmatorp = 0;
  k->p_hostile = (FED | ROM | ORI | KLI);
  k->p_war     = (k->p_hostile | k->p_swar);
}

/*
**  Return two team masks corresponding to the teams of the first two
**  teams found in the player list.
*/
static void sides ( int *one, int *two )
{
    struct player *k;
    int i;
    int unseen;

    unseen = (FED | ROM | ORI | KLI);
    *one = 0;
    *two = 0;
    k = &players[0];
    for(i=0;i<MAXPLAYER;i++)
    {
        if ( (k->p_status != PFREE) && (!(k->p_flags & PFROBOT)))
        {
            if ( ( unseen & k->p_team ) != 0 )
            {
                if ( *one == 0 )
                {
                    *one = k->p_team;
                    unseen &= ~k->p_team;
                    k++;
                    continue;
                }
                *two = k->p_team;
                return;
            }
        }
        k++;
    }
}

/*
**  Calculate a player value
*/
static int value ( struct player *k )
{
    return
    (int)
    (
        (float)
        (
#ifdef LTD_STATS
            ltd_bombing_rating(k) +
            ltd_planet_rating(k) +
            ltd_offense_rating(k)
#else
            bombingRating(k) +
            planetRating(k) +
            offenseRating(k)
#endif
        ) * 100.0
    );
}

/*
**  Balance the teams
**
**  Uses an exhaustive algorithm (I'm exhausted!) to find the best combination
**  of the current players that balances the teams in terms of statistics.
**  The algorithm will support only the number of players that fits into the
**  number of bits in an int.
**
**  If there are multiple "best" combinations, then the combination
**  involving the least number of team swaps will be chosen.
*/
int do_balance(void)
{
    int i, j;                   /* miscellaneous counters    */
    int records;                /* number of players in game */
    int one;                    /* team number one mask      */
    int two;                    /* team number two mask      */

    struct player *k;           /* pointer to current player */

    struct item
    {
        int p_no;               /* player number             */
        int p_value;            /* calculated player value   */
        int p_team;             /* team player on previously */
    } list[MAXPLAYER];          /* working array             */

    struct
    {
        int combination;        /* combination number        */
        int value;              /* team balance difference   */
        int one;                /* team one total value      */
        int two;                /* team two total value      */
        int swaps;              /* number of swaps involved  */
    } best;                     /* best team combination     */

    /* which teams are playing?  give up if only one found */
    sides ( &one, &two );
    if ( two == 0 )
    {
        pmessage ( 0, MALL, "GOD->ALL",
          "Can't balance only one team!" );
        pmessage ( 0, MALL, "GOD->ALL",
          "Please could somebody move to another team, then all vote again?" );
        return 0;
    }

    /* initialise best to worst case */
    best.combination = -1;
    best.value = 1<<30;
    best.one = 0;
    best.two = 0;
    best.swaps = 1<<30;

    /* reset working array */
    for(i=0;i<MAXPLAYER;i++)
    {
        list[i].p_no    = 0;
        list[i].p_value = 0;
    }

    /* insert players in working array */
    records = 0;
    k = &players[0];
    for(i=0;i<MAXPLAYER;i++)
    {
        if ( (k->p_status != PFREE) && (!(k->p_flags & PFROBOT)))
        {
            list[records].p_no    = k->p_no;
            list[records].p_value = value ( k );
            list[records].p_team  = k->p_team;
            records++;
        }
        k++;
    }

    /* randomise the working array; may cause different team mixes */
    for(i=0;i<records;i++)
    {
        int a, b;
        struct item swapper;

        a = random() % records;
        b = random() % records;

        swapper = list[a];
        list[a] = list[b];
        list[b] = swapper;
    }

    /* loop for every _possible_ combination to find the best */
    for(i=0;i<(1<<records);i++)
    {
        int difference;         /* difference in team total       */
        int value_a, value_b;   /* total stats per team           */
        int count_a, count_b;   /* total count of players on team */
        int swaps;              /* number of swaps involved       */

        /* if this a shadow combination already considered, ignore it */
        /* if ( ( i ^ ( ( 1<<records ) - 1 ) ) < i ) continue; */
        /* disabled - it will interfere with swap minimisation goal */

        /* is this combination an equal number of players each side? */
        count_a = 0;
        count_b = 0;

        for(j=0;j<records;j++)
            if((1<<j)&i)
                count_a++;
            else
                count_b++;

        /* skip this combination if teams are significantly unequal */
        if ( abs ( count_a - count_b ) > 1 ) continue;

        /* reset team total for attempt */
        value_a = 0;
        value_b = 0;

        /* calculate team total stats */
        for(j=0;j<records;j++)
            if((1<<j)&i)
                value_a += list[j].p_value;
            else
                value_b += list[j].p_value;

        /* calculate number of swaps this combination produces */
        swaps = 0;
        for(j=0;j<records;j++)
            if((1<<j)&i)
            {
                if ( list[j].p_team != one ) swaps++;
	    }
            else
	    {
                if ( list[j].p_team != two ) swaps++;
	    }

        /* calculate difference in team total stats */
        difference = abs ( value_a - value_b );

        /* if this combo is better than the previous one we had,
           or the combo is the same and the number of swaps is lower...  */
        if ( ( difference < best.value )
            || ( ( difference == best.value )
                && ( swaps < best.swaps ) ) )
        {
            /* remember it */
            best.value = difference;
            best.combination = i;
            best.one = value_a;
            best.two = value_b;
            best.swaps = swaps;
        }
    }

    /* announce movements intended */
    for(j=0;j<records;j++)
        if ( (1<<j)&best.combination )
            moveallmsg ( list[j].p_no, one, two );

    for(j=0;j<records;j++)
        if ( !((1<<j)&best.combination) )
            moveallmsg ( list[j].p_no, two, one );

    /* move players to their teams */
    for(j=0;j<records;j++)
        if ( (1<<j)&best.combination )
            move ( list[j].p_no, one, two );
        else
            move ( list[j].p_no, two, one );

    /* advise all of resultant team mix difference */
    pmessage ( 0, MALL, "GOD->ALL",
        "The %s total rating will be %.2f",
        teamNames[one],
        (float) ( best.one / 100.0 ) );

    pmessage ( 0, MALL, "GOD->ALL",
        "The %s total rating will be %.2f",
        teamNames[two],
        (float) ( best.two / 100.0 ) );

    return 0;
}

int do_triple_planet_mayhem(void)
{
    int i;

    struct player* j;

    /* balance the teams */
    do_balance();

    /* move all planets off the galaxy */
    for (i=0; i<MAXPLANETS; i++)
    {
        planets[i].pl_flags = 0;
        planets[i].pl_owner = 0;
        planets[i].pl_x = -10000;
        planets[i].pl_y = -10000;
        planets[i].pl_info = 0;
        planets[i].pl_armies = 0;
        strcpy ( planets[i].pl_name, "" );
    }

    /* disable Klingon and Orion teams; stop people from joining them */
    planets[20].pl_couptime = 999999; /* no Klingons */
    planets[30].pl_couptime = 999999; /* no Orions */

    /* initialise earth */
    i = 0;
    planets[i].pl_flags |= FED | PLHOME | PLCORE | PLAGRI | PLFUEL | PLREPAIR;
    planets[i].pl_x = 40000;
    planets[i].pl_y = 65000;
    planets[i].pl_armies = 40;
    planets[i].pl_info = FED;
    planets[i].pl_owner = FED;
    strcpy ( planets[i].pl_name, "Earth" );

    /* initialise romulus */
    i = 10;
    planets[i].pl_flags |= ROM | PLHOME | PLCORE | PLAGRI | PLFUEL | PLREPAIR;
    planets[i].pl_x = 40000;
    planets[i].pl_y = 35000;
    planets[i].pl_armies = 40;
    planets[i].pl_info = ROM;
    planets[i].pl_owner = ROM;
    strcpy ( planets[i].pl_name, "Romulus" );

    /* initialise indi */
    i = 18;
    planets[i].pl_flags |= PLFUEL | PLREPAIR;
    planets[i].pl_flags &= ~PLAGRI;
    planets[i].pl_x = 15980;
    planets[i].pl_y = 50000;
    planets[i].pl_armies = 4;
    planets[i].pl_info &= ~ALLTEAM;
    strcpy ( planets[i].pl_name, "Indi" );

    /* fix all planet name lengths */
    for (i=0; i<MAXPLANETS; i++)
    {
        planets[i].pl_namelen = strlen(planets[i].pl_name);
    }

    /* advise players */
    {
        char *list[] =
        {
	    "Galaxy reset for triple planet mayhem!",
	    "Rule 1: they take Indi, they win,",
	    "Rule 2: they take your home planet, they win,",
	    "Rule 3: you can't bomb Indi,",
	    "Rule 4: you may bomb their home planet, and;",
	    "Rule 5: all planets are FUEL & REPAIR, home planets are AGRI.",
	    ""
	};

	for ( i=0; strlen(list[i])!=0; i++ )
            pmessage ( 0, MALL, "GOD->ALL", list[i] );
    }
    return 0;
}
#endif /* TRIPLE_PLANET_MAYHEM */


#if defined (ALLOW_EJECT)
int do_player_eject(int who, int player, int mflags, int sendto)
{
    register struct player *j;

    j = &players[player];

    if (j->p_status == PFREE){
	pmessage(sendto, mflags, addr_mess(who,MTEAM), 
		"That player is not in the game");
	return;
     }
    if (j->p_flags & PFROBOT) {
	pmessage(sendto, mflags, addr_mess(players[who].p_team,MTEAM), 
		"You cannot eject a robot, twinks!");
	return;
    }
    if (j->p_team != players[who].p_team){
      pmessage(players[who].p_team, MTEAM, 
	addr_mess(players[who].p_team,MTEAM), 
	"You can only eject your own teammates, twinks!");
      return;
    }

    pmessage(0, MALL, addr_mess(who,MALL), 
	" %2s has been ejected by the players", j->p_mapchars);

#if defined (DOG)
    reset_player(j->p_no);
    dont_score(j->p_no);
#endif

    eject_player(j->p_no);
    if (freeslot(j) < 0) {
      ERROR(1,("do_player_eject: freeslot failed\n"));
    }
}

eject_player(int who)
{
  struct player* j;

  j = &players[who];
  j->p_ship.s_type = STARBASE;
  j->p_whydead=KQUIT;
  j->p_explode=10;
  j->p_status=PEXPLODE;
  j->p_whodead=me->p_no;
}
#endif /* ALLOW_EJECT */

#ifdef VOTING

check_listing(comm)
char *comm;
{
    register int i;

    for (i=1; i < NUM_VOTES; i++) {
	if (check_possible(comm,votes[i].type)!=NULL)
	  return i;
    }
    return 0;
}

int do_generic_vote(char *comm, int who)
{
    int num;
    int what = 0;
    int player = -1;
    register int i;
    register struct player *j;
    char *current;
    int pcount=0;  /* Total players in game */
    int vcount=0;  /* Number who've voted recently */
    int mflag = 0;
    int sendto = 0;

    if (!(num = check_listing(comm)) ) return 0;

#ifdef OBSERVERS
    if (players[who].p_status == POBSERV) {
      bounce(who,"Sorry, Observers can't vote");
      return 0;
    }
#endif

    if ( votes[num].tag & VC_GLOG ){
      if (glog_open() == 0) {	/* Log votes for God to see */
        time_t    t = time(NULL);
        char      tbuf[40];
        struct tm *tmv = localtime(&t);
      
#ifdef STRFTIME
	sprintf(tbuf, "%02d/%02d %d/%d", tmv->tm_hour,tmv->tm_min,
		tmv->tm_mday,tmv->tm_mon);
#else
	strftime(tbuf, 39, "%R %D", tmv);
#endif
        glog_printf("VOTE: %s %s@%s \"%s\"\n", tbuf, players[who].p_login, 
		    players[who].p_ip, comm);
      }
    }

    current = strstr(comm,votes[num].type);

    if (votes[num].tag & VC_PLAYER) {
	if ((what = getplayer(who,current)) < 0) return 0;
	player = what;
    }

    what += votes[num].start;

    if (what >= PV_TOTAL) return 0;

    if (votes[num].tag & VC_TEAM){
      sendto = players[who].p_team;
      mflag = MTEAM;
    }
    else if (votes[num].tag & VC_ALL){
      sendto = 0;
      mflag = MALL;
    }
    else {
	ERROR(1,("Unrecognized message flag in do_generic_vote() for %s\n",
		current));
	return 0;
    }

    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
        /*Skip free slots and robots */
        if ( (j->p_status == PFREE) || (j->p_flags & PFROBOT))  continue;


	/*Also skip players that are not on the same team if team flag is set*/
	if ((votes[num].tag & VC_TEAM) && (j->p_team != players[who].p_team))
	    continue; 

#ifdef OBSERVERS
	/* Also skip observers */
	if (j->p_status == POBSERV) continue;
#endif

#ifdef OBSERVERS
	/* Also skip observers */
	if (j->p_status == POBSERV) continue;
#endif

        pcount++;
        if (i==who) {
	    if ((j->voting[what] != 0) && (votes[num].frequency != 0)) {
		if ( (j->voting[what] + votes[num].frequency) > time(NULL) ) {
      		    bounce(who,"Sorry, you can only use %s every %1.1f minutes",
				votes[num].type, votes[num].frequency / 60.0);
	 	    return 0;
		}
	    }
            j->voting[what] = time(NULL);  /* Enter newest vote */
	}
        if (j->voting[what] > 0) vcount++;
    }

    pmessage(sendto, mflag, addr_mess(sendto,mflag),
	"%s has voted to %s. %d player%s (of %d) %s voted.",
	players[who].p_mapchars, current, vcount, vcount==1 ? "":"s",
        pcount, vcount==1 ? "has":"have");


    /* The Votes Passes */
    if ( (vcount >= votes[num].minpass) &&
       ( ((pcount < (2*vcount)) && (votes[num].tag & VC_ALL)) || 
         ((votes[num].tag & VC_TEAM) && (pcount-1 <= vcount)) )){
      pmessage(sendto, mflag, addr_mess(sendto,mflag), 
             "The motion %s passes", current);
      if ( votes[num].tag & VC_GLOG ){
        if (glog_open() == 0) {	/* Log pass for God to see */
 	  glog_printf("VOTE: The motion %s passes\n", current);
	}
      }
      
      /* Reset the Votes for this item since it passed */
      for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) 
        j->voting[what] = -1;
      
      if (votes[num].tag & VC_PLAYER) 
        (*votes[num].handler)(who, player, mflag, sendto);
      else
        (*votes[num].handler)(who, mflag, sendto);
      
    } else {
        if (votes[num].tag & VC_TEAM) 
          i = pcount - vcount - 1;
        else
          i = pcount/2 + 1 - vcount;
      
        if (i< (votes[num].minpass - vcount))
          i=votes[num].minpass - vcount;

        pmessage(sendto, mflag, addr_mess(sendto,mflag),
               "Still need %d more vote%s %s",i,i==1 ? " ":"s",current);
    }
    return 1;
}
#endif /* VOTING */

#if defined(AUTO_PRACTICE) && !defined(BASEP)
int do_start_basep(who, mflags, sendto)
int who, mflags,sendto;
{
  if (vfork() == 0) {
      (void) SIGNAL(SIGALRM,SIG_DFL);
      execl(Basep, "basep", 0);
      perror(Basep);
      }
}
#endif

#if defined(AUTO_INL) && !defined(INL)
int do_start_inl(who, mflags, sendto)
int who, mflags, sendto;
{
  if (vfork() == 0) {
      (void) SIGNAL(SIGALRM,SIG_DFL);
      execl(Inl, "inl", 0);
      perror(Inl);
      }
}
#endif

#if defined(AUTO_HOCKEY) && !defined(PUCK)
int do_start_puck(who, mflags, sendto)
int who, mflags, sendto;
{
  if (vfork() == 0) {
      (void) SIGNAL(SIGALRM,SIG_DFL);
      execl(Puck, "puck", 0);
      perror(Puck);
      }
}
#endif

#if defined(AUTO_DOGFIGHT) && !defined(DOG)
int do_start_mars(who, mflags, sendto)
int who, mflags, sendto;
{
  if (vfork() == 0) {
      (void) SIGNAL(SIGALRM,SIG_DFL);
      execl(Mars, "mars", 0);
      perror(Mars);
      }
}
#endif

char *addr_mess(who,type)
int who,type;
{
    static char addrbuf[10];
    char* team_names[MAXTEAM+1] = { "", "FED", "ROM", "", "KLI",
                                 "", "", "", "ORI" };

    if (type == MALL) 
      sprintf(addrbuf,"%s->ALL", myname);
    else if (type == MTEAM)
      sprintf(addrbuf,"%s->%3s", myname, team_names[who]);
    else /* Assume MINDIV */
      sprintf(addrbuf, "%s->%2s", myname, players[who].p_mapchars);

    return (addrbuf);
}


/* ARGSUSED */
int do_help(comm,mess)
char *comm;
struct message *mess;
{
    int who;
    int i;
    char *addr;

    who = mess->m_from;
    addr = addr_mess(who,MINDIV); 

#ifdef DOG			/* DOGFIGHTING */

    pmessage(who,MINDIV,addr, "This is the Dogfight Challenge.");

#elif defined(PUCK)		/* HOCKEY (PUCK) */

    pmessage(who,MINDIV,addr, 
	"This is Netrek hockey.  Push me into the opposition's goal to score.");
    pmessage(who,MINDIV,addr, 
	"Goal lines are Sco/Lyr and Spi/Cas. No t-mode, no bombing/beaming.");
    pmessage(who,MINDIV,addr, 
	"After exploding, ship appears where it died +/- random offset.");
#ifdef SOCCER_SHOOT
    pmessage(who,MINDIV,addr, 
	"To shoot: Be nearest to me and pressor. Shot goes in dir of pressor.");
#else	
    pmessage(who,MINDIV,addr, 
	"To shoot: Be nearest to me and pressor. Shot dir is your ship's dir.");
#endif
    pmessage(who,MINDIV,addr, 
	"My shields go up if I'm shootable.  Easiest to shoot at low warp.");
    pmessage(who,MINDIV,addr, 
	"Shots can be blocked by tractoring/pressoring a shot puck.  No etmp.");
#ifdef OFFSIDE
    pmessage(who,MINDIV,addr, 
	"Offsides rules are similar to those in the NHL.");
#endif

#elif defined(BASEP) && defined (BASEPRACTICE)	/* BASEPRACTICE server */
    pmessage(who,MINDIV,addr,"Basepractice Rules are simple:");
    pmessage(who,MINDIV,addr,"Smack the Base or avoid it if you are a Base.");
    pmessage(who,MINDIV,addr,"Check the MOTD for detailed help.");

#elif defined(INL)		/* INL server */
    pmessage(who,MINDIV,addr,"INL server commands.");
 
#else				/* EVERYTHING ELSE */
#endif	

    if (NUM_COMMANDS > 1) { 
	pmessage(who,MINDIV,addr, "Possible commands are:");

	for (i=1; i < NUM_COMMANDS ; i++) {
	    pmessage(who,MINDIV,addr, "|%10s - %s",
		commands[i].command,commands[i].desc);
	}
    }
    
#ifdef VOTING
    if (NUM_VOTES > 1) {
	char ch;
        pmessage(who,MINDIV,addr,
		"The following votes can be used:  (M=Majority, T=Team vote)");
        pmessage(who,MINDIV,addr,
		"Ejection Votes are recorded for the god to review.");

	for (i=1; i<NUM_VOTES; i++) {
	    if (votes[i].tag & VC_TEAM) ch = 'T';
	    else ch = 'M';
	    pmessage(who,MINDIV,addr, "|%10s - %c: %s",
		votes[i].type, ch, votes[i].desc);
	}
    }
#endif
}

#ifndef INL

/*** QUERIES ***/
send_query(which,who,from)
char which;
int  who, from;
{
	pmessage2(who, MINDIV, " ", from, "%c", which);
}

/* ARGSUSED */
do_client_query(comm,mess,who)
char *comm;
struct message *mess;
int who;
{
#ifdef RSA
	send_query('#', who, mess->m_from); 
#endif
}

/* ARGSUSED */
do_ping_query(comm,mess,who)
char *comm;
struct message *mess;
int who;
{
      send_query('!', who, mess->m_from);
}

/* ARGSUSED */
do_stats_query(comm,mess,who)
char *comm;
struct message *mess;
int who;
{
      send_query('?', who, mess->m_from); 
}

/* ARGSUSED */
do_whois_query(comm,mess,who)
char *comm;
struct message *mess;
int who;
{
      send_query('@', who, mess->m_from);
}

#ifdef RSA
int bounceRSAClientType(from)
int from;
{
        bounce(from,"Client: %s", RSA_client_type);

        return 1;
}
#endif

int bounceSessionStats(int from)
{
    float                          sessionBombing, sessionPlanets,
                                   sessionOffense, sessionDefense;
    int                            deltaArmies, deltaPlanets,
                                   deltaKills, deltaLosses,
                                   deltaTicks;

#ifdef LTD_STATS
    deltaPlanets = ltd_planets_taken(me, LTD_TOTAL) - startTplanets;
    deltaArmies = ltd_armies_bombed(me, LTD_TOTAL) - startTarms;
    deltaKills = ltd_kills(me, LTD_TOTAL) - startTkills;
    deltaLosses = ltd_deaths(me, LTD_TOTAL) - startTlosses;
    deltaTicks = ltd_ticks(me, LTD_TOTAL) - startTticks;
#else
    deltaPlanets = me->p_stats.st_tplanets - startTplanets;
    deltaArmies = me->p_stats.st_tarmsbomb - startTarms;
    deltaKills = me->p_stats.st_tkills - startTkills;
    deltaLosses = me->p_stats.st_tlosses - startTlosses;
    deltaTicks = me->p_stats.st_tticks - startTticks;
#endif


    if (deltaTicks == 0) {
        bounce(from,
                "Session stats are only available during t-mode.");
        return 1; /* non t-mode */
    }

    sessionPlanets = (double ) deltaPlanets * status->timeprod /
        ((double) deltaTicks * status->planets);

    sessionBombing = (double) deltaArmies * status->timeprod /
        ((double) deltaTicks * status->armsbomb);

    sessionOffense = (double) deltaKills * status->timeprod /
        ((double) deltaTicks * status->kills);

    sessionDefense = (double) deltaTicks * status->losses /
        (deltaLosses!=0 ? (deltaLosses * status->timeprod) : (status->timeprod));

    bounce(from,
        "%2s stats: %d planets and %d armies. %d wins/%d losses. %5.2f hours.",
        me->p_mapchars,
        deltaPlanets,
        deltaArmies,
        deltaKills,
        deltaLosses,
        (float) deltaTicks/36000.0);
    bounce(from,
        "Ratings: Pla: %5.2f  Bom: %5.2f  Off: %5.2f  Def: %5.2f  Ratio: %4.2f",
        sessionPlanets,
        sessionBombing,
        sessionOffense,
        sessionDefense,
        (float) deltaKills /
        (float) ((deltaLosses == 0) ? 1 : deltaLosses));
    return 1;
}

int
bounceSBStats(from)
    int from;
{
    float                       sessionRatio, sessionKPH, sessionDPH,
                                overallKPH, overallDPH, overallRatio;
    int                         deltaKills, deltaLosses,
                                deltaTicks;

#ifdef LTD_STATS
    deltaKills = ltd_kills(me, LTD_SB) - startSBkills;
    deltaLosses = ltd_deaths(me, LTD_SB) - startSBlosses;
    deltaTicks = ltd_ticks(me, LTD_SB) - startSBticks;
    overallRatio = (float) (ltd_deaths(me, LTD_SB) == 0) ?
      (float) ltd_kills(me, LTD_SB) :
      (float) ltd_kills(me, LTD_SB) / (float) ltd_deaths(me, LTD_SB);
#else
    deltaKills = me->p_stats.st_sbkills - startSBkills;
    deltaLosses = me->p_stats.st_sblosses - startSBlosses;
    deltaTicks = me->p_stats.st_sbticks - startSBticks;
    overallRatio = (float) (me->p_stats.st_sblosses==0) ?
      (float) me->p_stats.st_sbkills :
      (float) me->p_stats.st_sbkills / (float) me->p_stats.st_sblosses;
#endif

/*  if (deltaTicks == 0) {
        bounce(from,
        "No SB time yet this session.");
        return 0;
    } */

    if (deltaTicks != 0) {
        sessionKPH = (float) deltaKills * 36000.0 / (float) deltaTicks;
        sessionDPH = (float) deltaLosses * 36000.0 / (float) deltaTicks;
    } else {
        sessionKPH = sessionDPH = 0.0;
    }

    sessionRatio = (float) (deltaLosses==0) ? (float) deltaKills :
        (float) deltaKills / (float) deltaLosses;

#ifdef LTD_STATS
    if (ltd_ticks(me, LTD_SB) != 0) {
        overallKPH = (float) ltd_kills(me, LTD_SB) * 36000.0 /
                     (float) ltd_ticks(me, LTD_SB);
        overallDPH = (float) ltd_deaths(me, LTD_SB) * 36000.0 /
                     (float) ltd_ticks(me, LTD_SB);
#else
    if (me->p_stats.st_sbticks != 0) {
        overallKPH = (float) me->p_stats.st_sbkills * 36000.0 /
                (float) me->p_stats.st_sbticks;
        overallDPH = (float) me->p_stats.st_sblosses * 36000.0 /
                (float) me->p_stats.st_sbticks;
#endif

    } else {
        overallKPH = overallDPH = 0.0;
    }

    bounce(from,
      "%2s overall SB stats: %d wins/%d losses. %5.2f hours. Ratio: %5.2f",
      me->p_mapchars,
#ifdef LTD_STATS
      ltd_kills(me, LTD_SB),
      ltd_deaths(me, LTD_SB),
      (float) ltd_ticks(me, LTD_SB) / 36000.0,
#else
      me->p_stats.st_sbkills,
      me->p_stats.st_sblosses,
      (float) me->p_stats.st_sbticks/36000.0,
#endif
      overallRatio);

    if (deltaTicks)
        bounce(from,
          "%2s session SB stats: %d wins/%d losses. %5.2f hours. Ratio: %5.2f",
          me->p_mapchars,
          deltaKills,
          deltaLosses,
          (float) deltaTicks/36000.0,
          sessionRatio);
        bounce(from,
          "Kills/Hour: %5.2f (%5.2f total), Deaths/Hour: %4.2f (%4.2f total)",
          sessionKPH,
          overallKPH,
          sessionDPH,
          overallDPH);
    return 1;
}


#ifdef PING
int bouncePingStats(from)
int from;
{
    if(me->p_avrt == -1){
        /* client doesn't support it or server not pinging */
        bounce(from,"No PING stats available for %c%c",
           me->p_mapchars[0], me->p_mapchars[1]);
    }
    else{
        bounce(from,
            "%c%c PING stats: Average: %d ms, Stdv: %d ms, Loss: %d%%/%d%% s->c/c->s",
            me->p_mapchars[0], me->p_mapchars[1],
            me->p_avrt,
            me->p_stdv,
            me->p_pkls_s_c,
            me->p_pkls_c_s);
    }
    return 1;
}
#endif


#if !defined (DOG) && !defined (PUCK) 		/* Server only */

/* ARGSUSED */
do_sbstats_query(comm,mess,who)
char *comm;
struct message *mess;
int who;
{
      send_query('^', who, mess->m_from);	
}

#ifdef GENO_COUNT
do_genos_query(comm,mess,who)
char *comm;
struct message *mess;
int who;
{
  char *addr;

  addr = addr_mess(mess->m_from,MINDIV);
  pmessage(mess->m_from, MINDIV, addr, "%s has won the game %d times.",
	players[who].p_name,players[who].p_stats.st_genos);
}
#endif


/* ARGSUSED */
#if !defined (BASEP) || !defined(BASEPRACTICE)
do_time_msg(comm,mess)
char *comm;
struct message *mess;
{
  char *teamNames[9] = {" ", "Federation", "Romulans", " ", "Klingons", 
			  " ", " ", " ", "Orions"};
  int who;
  int t;
  char *addr;

  who = mess->m_from;
  addr = addr_mess(who,MINDIV);

  for (t=0;((t<=MAXTEAM)&&(teams[t].s_surrender==0));t++);

  if (t>MAXTEAM) {
    pmessage(who, MINDIV, addr, "No one is considering surrender now.  Go take some planets.");
  } else {
    pmessage(who, MINDIV, addr, "The %s have %d minutes left before they surrender.", teamNames[t],teams[t].s_surrender);
  }
}

/*
 * Give information about the waitqueue status
 */

/* ARGSUSED */
do_queue_msg(comm,mess)
char *comm;
struct message *mess;
{
    int who;
    char *addr;
    int i;

    who = mess->m_from;
    addr = addr_mess(who,MINDIV);

    for (i=0; i < MAXQUEUE; i++) {
        /* Only report open queues that have the report flag set. */
        if (!(queues[i].q_flags & QU_OPEN))   continue;
	if (!(queues[i].q_flags & QU_REPORT)) continue;
      
	if (queues[i].count>0 ) 
  	    pmessage(who, MINDIV, addr, "Approximate %s queue size: %d", 
		     queues[i].q_name, queues[i].count);
	else
	    pmessage(who, MINDIV, addr, "There is no one on the %s queue.",
		     queues[i].q_name);
    }
}

#endif
#endif

#endif		/* INL */

/* 	$Id: inl.c,v 1.4 2006/05/06 12:28:20 quozl Exp $	 */

/*
 * inl.c
 *
 * INL server robot by Kurt Siegl
 * Based on basep.c
 *
 * Many improvements by Kevin O'Connor
 *
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "inldefs.h"
#include "proto.h"
#include "alarm.h"
#include "roboshar.h"
#include "ltd_stats.h"
#include "blog.h"
#include "util.h"
#include "planet.h"

/*

From: Tom Holub <doosh@best.com>
Date: Fri, 27 Apr 2001 20:37:06 -0700
Message-ID: <20010427203706.A17757@best.com>

Default regulation and overtime periods

*/
#define INL_REGULATION 60
#define INL_OVERTIME 20

int debug=0;

static int cambot_pid = 0;
static int obliterate_timer = 0;

struct planet *oldplanets;	/* for restoring galaxy */
#ifdef nodef
struct planet *inl_planets;
#endif
struct itimerval udt;

int	oldmctl;
FILE	*inl_log;

char time_msg[4][10] = {"seconds", "second", "minutes", "minute"};

Team	inl_teams[INLTEAM+1] = {
  {
    HOME,		/* Away or home team */
    "home",		/* away or home string */
    "Home",		/* Name of team */
    NONE,		/* who is captain */
    HOMETEAM,		/* Race flag */
    NOT_CHOOSEN,	/* index into side array */
    12,			/* armies requested to start with */
    INL_REGULATION,	/* requested length of game */
    INL_OVERTIME,	/* requested length of overtime */
    0,			/* team flags */
    1,			/* tmout counter */
    10,			/* number of planets owned */
    0, 0, 0		/* continuous scores */

  },
  {
    AWAY,		/* Away or home team */
    "away",		/* away or home string */
    "Away",		/* Name of team */
    NONE,		/* who is captain */
    AWAYTEAM,		/* Race flag */
    NOT_CHOOSEN,	/* index into side array */
    12,			/* armies requested to start with */
    INL_REGULATION,	/* requested length of game */
    INL_OVERTIME,	/* requested length of overtime */
    0,			/* team flags */
    1,			/* tmout counter */
    10,			/* number of planets owned */
    0, 0, 0		/* continuous scores */
  }
};

Sides	sides[MAXTEAM+1] = { {"FED", FED, 2},
			     {"ROM", ROM, 3},
			     {"KLI", KLI, 0},
			     {"ORI", ORI, 1}};

int cbounds[9][4] =
{
  {0, 0, 0, 0},
  {0, 45000, 55000, 100000},
  {0, 0, 55000, 55000},
  {0, 0, 0, 0},
  {45000, 0, 100000, 55000},
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {0, 0, 0, 0},
  {45000, 45000, 100000, 100000}
};

/* define the name of the moderation bot - please note that due to the way */
/* messages are handled and the formatting of those messages care must be  */
/* taken to ensure that the bot name does not exceed 5 characters.  If the */
/* desired name is larger than 5 chars the message routines will need to   */
/* have their formatting and contents corrected */
char	*roboname = "INL";
char	*inl_from = {"INL->ALL"};

Inl_stats inl_stat = {
  12,			/* start_armies */
  1,			/* change */
  S_PREGAME,		/* flags */
  0,			/* ticks */
  0,			/* game_ticks */
  0,			/* tmout_ticks */
  0,			/* time */
  0,			/* overtime */
  0,			/* extratime */
  0,			/* remaining */
  0,			/* score_mode */
  0.0,			/* weighted_divisor */
  {-1, -1},		/* swap */
  0,			/* swapnum */
  -1			/* swapteam */
};

Inl_countdown inl_countdown =
{0,{60,30,10,5,4,3,2,1,0,-1},8,0,NULL,""};

int end_tourney();
int checkgeno();
Inl_countdown inl_game =
{0,{2700,1800,600,300,120,60,30,10,5,4,3,2,1,0,-1},12,0,end_tourney,""};

/* XXX Gag me! */
/* the four close planets to the home planet */
static int core_planets[4][4] =
{
  {7, 9, 5, 8,},
  {12, 19, 15, 16,},
  {24, 29, 25, 26,},
  {34, 39, 38, 37,},
};
/* the outside edge, going around in order */
static int front_planets[4][5] =
{
  {1, 2, 4, 6, 3,},
  {14, 18, 13, 17, 11,},
  {22, 28, 23, 21, 27,},
  {31, 32, 33, 35, 36,},
};

void cleanup();
void checkmess();
void inlmove();
int start_tourney();
void reset_stats();
void update_scores();
void announce_scores(int, int, FILE *);
void pl_reset_inl(int startup);
void countdown(int counter, Inl_countdown *cnt);
void obliterate(int wflag, char kreason);
void player_maint();
void init_server();
int all_alert(int stat);
int check_winner();

extern char *addr_mess(int who, int type);
extern char *strcasestr(const char *, const char *);


int
main(argc, argv)
     int	     argc;
     char	    *argv[];
{
  srandom(time(NULL));

  getpath();
  openmem(1);
  do_message_post_set(check_command);
  readsysdefaults();

  if ((inl_log = fopen(N_INLLOG,"w+"))==NULL) {
    ERROR(1,("Could not open INL log file.\n"));
    exit(1);
  }

  alarm_init();
  if (!debug)
    {
      signal(SIGINT, cleanup);
      signal(SIGTERM, cleanup);
    }

  oldmctl = mctl->mc_current;
#ifdef nodef
  for (i = 0; i <= oldmctl; i++) {
    check_command(&messages[i]);
  }
#endif

  pmessage(0, MALL, inl_from, "###############################");
  pmessage(0, MALL, inl_from, "#  The INL robot has entered  #");
  pmessage(0, MALL, inl_from, "###############################");

  init_server();
  reset_inl(0);

  alarm_setitimer(distortion, fps);
  while ((status->gameup) & GU_GAMEOK) {
    alarm_wait_for();
    inlmove();
  }
  cleanup();
  return 0;
}

void
inl_freeze(void)
{
  int c;
  struct player *j;

  for (c=0; c < INLTEAM; c++)
    inl_teams[c].flags |= T_PAUSE;

  inl_stat.flags |= S_FREEZE;
  status->gameup |= (GU_PRACTICE | GU_PAUSED);

  for (j = firstPlayer; j <= lastPlayer; j++)
    {
      j->p_flags &= ~(PFSEEN);
      j->p_lastseenby = VACANT;	 /* if left non-vacant the daemon might */
    }				 /* not reinitalize PFSEEN automaticly */

  pmessage(0, MALL, inl_from, "-------- Game is paused -------");

}

void
inl_confine(void)
{
  struct player *j;

  for (j = firstPlayer; j <= lastPlayer; j++) {
    p_x_y_box(j, cbounds[j->p_team][0], cbounds[j->p_team][1],
                 cbounds[j->p_team][2], cbounds[j->p_team][3]);
  }
}

void
inl_unconfine(void)
{
  struct player *j;

  for (j = firstPlayer; j <= lastPlayer; j++) {
    p_x_y_unbox(j);
  }
}

typedef struct playerlist {
    char name[NAME_LEN];
    char mapchars[2];
    int planets, armies;
} Players;


static void displayBest(FILE *conqfile, int team, int type)
{
    register int i,k,l;
    register struct player *j;
    int planets, armies;
    Players winners[MAXPLAYER+1];
    char buf[MSG_LEN];
    int number;

    number=0;
    memset(winners, 0, sizeof(Players) * (MAXPLAYER+1));
    for (i = 0, j = &players[0]; i < MAXPLAYER; i++, j++) {
        if (j->p_team != team || j->p_status == PFREE) continue;
#ifdef GENO_COUNT
        if (type==KWINNER) j->p_stats.st_genos++;
#endif
        if (type == KGENOCIDE) {
            planets=j->p_genoplanets;
            armies=j->p_genoarmsbomb;
            j->p_genoplanets=0;
            j->p_genoarmsbomb=0;
        } else {
            planets=j->p_planets;
            armies=j->p_armsbomb;
        }
        for (k = 0; k < number; k++) {
            if (30 * winners[k].planets + winners[k].armies < 
                30 * planets + armies) {
	      /* insert person at location k */
                break;
            }
        }
        for (l = number; l >= k; l--) {
            winners[l+1] = winners[l];
        }
        number++;
        winners[k].planets=planets;
        winners[k].armies=armies;
        strncpy(winners[k].mapchars, j->p_mapchars, 2);
        strncpy(winners[k].name, j->p_name, NAME_LEN);
        winners[k].name[NAME_LEN-1]=0;  /* `Just in case' paranoia */
    }
    for (k=0; k < number; k++) {
      if (winners[k].planets != 0 || winners[k].armies != 0) {
            sprintf(buf, "  %16s (%2.2s) with %d planets and %d armies.", 
                winners[k].name, winners[k].mapchars, winners[k].planets, winners[k].armies);
            pmessage(0, MALL | MCONQ, " ",buf);
            fprintf(conqfile, "  %s\n", buf);
      }
    }
    return;
}


static void genocideMessage(int loser, int winner)
{
    char buf[MSG_LEN];
    FILE *conqfile;
    time_t curtime;

    conqfile=fopen(ConqFile, "a");

    /* TC 10/90 */
    time(&curtime);
    strcpy(buf,"\nGenocide! ");
    strcat(buf, ctime(&curtime));
    fprintf(conqfile,"  %s\n",buf);

    pmessage(0, MALL | MGENO, " ","%s",
        "***********************************************************");
    sprintf(buf, "%s has been genocided by %s.", inl_teams[loser].t_name,
       inl_teams[winner].t_name);
    pmessage(0, MALL | MGENO, " ","%s",buf);
        
    fprintf(conqfile, "  %s\n", buf);
    sprintf(buf, "%s:", inl_teams[winner].t_name);
    pmessage(0, MALL | MGENO, " ","%s",buf);
    fprintf(conqfile, "  %s\n", buf);
    displayBest(conqfile, inl_teams[winner].side, KGENOCIDE);
    sprintf(buf, "%s:", inl_teams[loser].t_name);
    pmessage(0, MALL | MGENO, " ","%s",buf);
    fprintf(conqfile, "  %s\n", buf);
    displayBest(conqfile, inl_teams[loser].side, KGENOCIDE);
    pmessage(0, MALL | MGENO, " ","%s",
        "***********************************************************");
    fprintf(conqfile, "\n");
    fclose(conqfile);
}


int 
checkgeno()
{
    register int i;
    register struct planet *l;
    struct player *j;
    int loser, winner;

    for (i = 0; i <= MAXTEAM; i++) /* maint: was "<" 6/22/92 TC */
        teams[i].te_plcount = 0;

    for (i = 0, l = &planets[i]; i < MAXPLANETS; i++, l++) {
        teams[l->pl_owner].te_plcount++; /* for now, recompute here */
    }

    if (teams[inl_teams[HOME].side].te_plcount == 0) 
      {
	loser = HOME;
	winner = AWAY;
      }
    else if (teams[inl_teams[AWAY].side].te_plcount == 0)
           {       
	     loser = AWAY;
	     winner = HOME;
	   }
         else
           return 0;


    /* Tell winning team what wonderful people they are (pat on the back!) */
    genocideMessage(loser, winner);
    /* Give more troops to winning team? */

    /* kick out all the players on that team */
    /* makes it easy for them to come in as a different team */
    for (i = 0, j = &players[0]; i < MAXPLAYER; i++, j++) {
      if (j->p_status == PALIVE && j->p_team == loser) {
	j->p_status = PEXPLODE;
	if (j->p_ship.s_type == STARBASE)
	  j->p_explode = 2*SBEXPVIEWS/PLAYERFUSE;
	else
	  j->p_explode = 10/PLAYERFUSE;
	j->p_whydead = KGENOCIDE;
	j->p_whodead = inl_teams[winner].captain;
      }
    }
    return 1;
}

void
inlmove()
{
  int c;

  /***** Start The Code Here *****/

#ifdef INLDEBUG
  ERROR(2,("Enter inlmove\n"));
#endif

  inl_stat.ticks++;
  player_maint();		      /* update necessary stats and info */

  if (inl_stat.flags & S_COUNTDOWN)
    countdown(inl_stat.ticks,&inl_countdown);
  checkmess();

  update_sys_defaults();

  if (inl_stat.flags & S_CONFINE && (!(inl_stat.flags & S_DRAFT) || (status->gameup & GU_INL_DRAFTED)))
    inl_confine();
  else
    inl_unconfine();

  if (!(inl_stat.flags & (S_TOURNEY | S_OVERTIME)) ||
      (inl_stat.flags & S_FREEZE )) {
    return;
  }

  inl_stat.game_ticks++;
  inl_stat.remaining = inl_stat.time +
    (inl_stat.flags & S_OVERTIME ? inl_stat.overtime : 0) +
    inl_stat.extratime - inl_stat.game_ticks;
  context->inl_game_ticks = inl_stat.game_ticks;
  context->inl_remaining = inl_stat.remaining;

  if (obliterate_timer > 0)
    obliterate_timer--;

  /* check for timeout */
  if (inl_stat.flags & S_TMOUT)
    {
      inl_stat.tmout_ticks++;
      if (!obliterate_timer &&
          ((inl_stat.tmout_ticks > 1300) || all_alert(PFGREEN)
	  || ((inl_stat.tmout_ticks > 800) && all_alert(PFYELLOW))))
	/* This is a loose approximation
	   of the INL source code.. */
	{
	  inl_stat.flags &= ~(S_TMOUT);
	  inl_stat.tmout_ticks = 0;

	  for (c=0; c < INLTEAM; c++)
	    inl_teams[c].flags &= ~(T_TIMEOUT);

	  inl_freeze();
	}
    }

  status->tourn = 1;		/* paranoia? */
  tournplayers = 1;
#ifdef nodef
  if (!(inl_stat.game_ticks % PLANETFUSE))
    checkplanets();
#endif

  /* check for sudden death overtime */
  if ((inl_stat.flags & S_OVERTIME) && (inl_stat.game_ticks % PERSEC))
    end_tourney();

  /* display periodic countdown */
  if (!(inl_stat.game_ticks % TIMEFUSE))
    countdown(inl_stat.game_ticks, &inl_game);

  /* update continuous scoring */
  update_scores();

  /* check for geno and end game if there is */
  if (checkgeno()) end_tourney();

#ifdef INLDEBUG
  ERROR(2,("	game ticks = %d\n", inl_stat.game_ticks));
#endif
}

void player_maint()
{
#ifdef INLDEBUG
  ERROR(2,("Enter player_maint\n"));
#endif
  /* Remove the captain flag if the captain disappeared */
  if (inl_teams[HOME].captain >= 0)
    if (players[inl_teams[HOME].captain].p_status == PFREE) {
      inl_teams[HOME].captain = NONE;
      players[inl_teams[HOME].captain].p_inl_captain = 0;
    }
  if (inl_teams[AWAY].captain >= 0)
    if (players[inl_teams[AWAY].captain].p_status == PFREE) {
      inl_teams[AWAY].captain = NONE;
      players[inl_teams[AWAY].captain].p_inl_captain = 0;
    }
}

int all_alert(int stat)
{
  struct player *j;

  for (j = firstPlayer; j <= lastPlayer; j++)
    if ((j->p_status == PALIVE) && !(j->p_flags & stat))
      return 0;

  return 1;
}

void logmessage(struct message *m)
{
  /* decide whether or not to log this message */
#ifdef nodef
  if (m->m_flags & MINDIV) return; /* individual message */
  if (!(m->m_flags & MGOD)) return;
  if ((m->m_flags & MGOD) > least) return;
#endif
  if (strcasestr(m->m_data, "ADMIN PASSWORD") == NULL) {
    fprintf(inl_log,"%5d: %s\n",inl_stat.ticks,m->m_data);
  }
}

void checkmess()
{
#ifdef INLDEBUG
  ERROR(2,("Enter checkmess\n"));
#endif

  /* exit if daemon terminates use of shared memory */
  if (forgotten()) exit(1);

  while (oldmctl != mctl->mc_current) {
    oldmctl++;
    if (oldmctl == MAXMESSAGE) oldmctl = 0;
    /* no commands during countdown, and no buffering of them */
    if (!(inl_stat.flags & S_COUNTDOWN) &&
        messages[oldmctl].m_flags & MINDIV) {
      if (messages[oldmctl].m_recpt == messages[oldmctl].m_from) {
	me = &players[messages[oldmctl].m_from];
	if (!check_command(&messages[oldmctl]))
	{
      if (!me->p_verify_clue)
	    pmessage(messages[oldmctl].m_from, MINDIV,
		     addr_mess(messages[oldmctl].m_from, MINDIV),
		     "Not an INL command.  Send yourself 'help' for help.");
      else
        me->p_verify_clue = 0;
        }
      }
    }
#ifdef nodef
    else if (messages[oldmctl].m_flags & MALL) {
      if (strstr(messages[oldmctl].m_data, "help") != NULL)
	pmessage(messages[oldmctl].m_from, MINDIV,
		 addr_mess(messages[oldmctl].m_from, MINDIV),
		 "If you want help, send the message 'help' to yourself.");
    }
#endif
    if (messages[oldmctl].m_flags == (MTEAM | MDISTR | MVALID)) {
      struct message msg;
      int size;
      struct distress dist;
      char buf[MSG_LEN];

      memcpy(&msg, &messages[oldmctl], sizeof(struct message));
      buf[0]='\0';
      msg.m_flags ^= MDISTR;
      HandleGenDistr(msg.m_data,msg.m_from,msg.m_recpt,&dist);
      size = makedistress(&dist,buf,distmacro[dist.distype].macro);
      strncpy(msg.m_data,buf,size+1);
      logmessage(&(msg));
    } else {
      logmessage(&(messages[oldmctl]));
    }
  }
}

void
countplanets()
{
  int i, c;

  for (i=0; i < INLTEAM; i++)
    inl_teams[i].planets = 0;
  for (c=0; c < MAXPLANETS; c++ )
    for (i=0; i < INLTEAM; i++)
      if (sides[inl_teams[i].side_index].flag == planets[c].pl_owner)
	inl_teams[i].planets++;
}

/* continuous scoring    -da */
void update_scores() {

  int p, t;
  double weight = (double) inl_stat.game_ticks / (double) inl_stat.time;

  static const double weight_max = 2.0;  /* exp(1.0) - 0.71828183 */

  /* sanity check! */
  if (weight < 0.0)
    weight = 0.0;
  else if (weight > 1.0)
    weight = 1.0;

  /* weight = linear range between 0.0 and 1.0 during regulation.
   * map this to exponential curve using formula:
   *    weight = exp(time) - (0.71828183 * time)
   *    weight range is 1.0 to 2.0
   * -da */

  weight = exp(weight) - (0.71828183 * weight);

  /* sanity check */
  if (weight < 1.0)
    weight = 1.0;
  else if (weight > weight_max)
    weight = weight_max;

  /* weight multiply factor should be max points you can get per planet */
  inl_stat.weighted_divisor += (weight * 2.0);

  for (p=0; p<MAXPLANETS; p++) {

    for(t=0; t<INLTEAM; t++) {

      if (sides[inl_teams[t].side_index].flag == planets[p].pl_owner) {

        /* planet is owned by this team, so it has > 0 armies.  each planet
           is worth 1-2 points for each pair of armies on the planet,
           capped at 2.  I.e. 1-2 armies = 1 pt, >=3 armies = 2 pts. */

        int points = planets[p].pl_armies;

        if (points > 2)
          points = 2;
        else
          points = 1;

        /* absolute continuous score is just the cumulative point total */
        inl_teams[t].abs_score += points;

        /* semi-continuous score is just the cumulative point total with
           a 1 minute interval */
        if ((inl_stat.game_ticks % PERMIN) == 0)
          inl_teams[t].semi_score += points;

        /* weighted continuous score is a cumulative total of points
           measured in a linearly decreasing interval */
        inl_teams[t].weighted_score += (weight * (double) points);

      }

    }

  }

  /* announce updated scores every 5 minutes if not in normal mode */

  if (inl_stat.score_mode && ((inl_stat.game_ticks % (5 * PERMIN)) == 0))
    announce_scores(0, MALL, NULL);

}

#ifdef nodef
checkplanets()
{
  int c;
  int size;
  int i;

#ifdef INLDEBUG
  ERROR(2,("Enter checkplanets\n"));
#endif

  size = sizeof(struct planet);

  for (c=0; c < MAXPLANETS; c++ ) {
    if (memcmp(&inl_planets[c], &planets[c], size) != 0) {
      /* Some planet information changed. Find out what it is and
	 report it. */

      /* pl_no, & pl_flags should never change */
      /* pl_x, pl_y, pl_name, pl_namelen should never change in INL */
      /* pl_deadtime & pl_couptime should not be of any interest */

      if (inl_planets[c].pl_owner != planets[c].pl_owner) {
	fprintf(inl_log,"PLANET: %s was taken by the %s from %s\n",
		inl_planets[c].pl_name, teamshort[planets[c].pl_owner],
		teamshort[inl_planets[c].pl_owner]);

	/*  Keep team[x].planets up to date */
	for (i=0; i < INLTEAM; i++)
	  {
	    if (sides[inl_teams[i].side_index].flag
		== inl_planets[c].pl_owner)
	      inl_teams[i].planets--;
	    if (sides[inl_teams[i].side_index].flag
		== planets[c].pl_owner)
	      inl_teams[i].planets++;
	  }

	inl_planets[c].pl_owner = planets[c].pl_owner;
      }
      if (inl_planets[c].pl_armies != planets[c].pl_armies) {
	if (inl_planets[c].pl_armies > planets[c].pl_armies)
	  fprintf(inl_log,
		  "PLANET: %s's armies have been reduced to %d\n",
		  inl_planets[c].pl_name,
		  planets[c].pl_armies);
	else
	  fprintf(inl_log,
		  "PLANET: %s's armies have increased to %d\n",
		  inl_planets[c].pl_name,
		  planets[c].pl_armies);
	inl_planets[c].pl_armies = planets[c].pl_armies;
      }
      if (inl_planets[c].pl_info != planets[c].pl_info) {
	int who, i;

	who = inl_planets[c].pl_info | planets[c].pl_info &
	  ~(inl_planets[c].pl_info & planets[c].pl_info);

	for (i = 0; i < INLTEAM; i++) {
	  if (who & sides[inl_teams[i].side_index].flag)
	    fprintf(inl_log,
		    "PLANET: %s has been touched by the %ss(%s)\n",
		    inl_planets[c].pl_name,
		    sides[inl_teams[i].side_index].name,
		    inl_teams[i].name);
	}
	inl_planets[c].pl_info = planets[c].pl_info;
      }
    }
  }
}
#endif

/* Determine if there is a winner at the end of regulation.
   returns -1 if momentum score goes to EXTRA TIME
   returns  0 if game is tied
   returns  1 if winner by planet count
   returns  2 if winner by continuous score > 2.0
   returns  3 if winner by continuous score < 2.0 & planet 11-8-1
   returns  4 if winner by momentum scoring
   -da
 */
int check_winner() {
  double divisor = inl_stat.weighted_divisor;
  double delta;

  /* -1 = HOME is winning
   *  0 = tied
   *  1 = AWAY is winning */
  int winning_cont = 0;
  int winning_norm = 0;

  countplanets();

  if (inl_teams[HOME].planets > inl_teams[AWAY].planets + 2)
    winning_norm = -1;
  else if (inl_teams[AWAY].planets > inl_teams[HOME].planets + 2)
    winning_norm = 1;
  else
    winning_norm = 0;

  /* NORMAL scoring mode; OR, continuous/momentum scoring and in OT.
     Use absolute planet count. */

  if ((inl_stat.score_mode == 0) || (inl_stat.flags & S_OVERTIME)) {
    if (winning_norm)
      return 1;
    else
      return 0;
  }

  /* CONTINUOUS or MOMENTUM scoring mode */

  /* sanity check */
  if (inl_stat.weighted_divisor < 1.0)
    return 0;

  if (inl_stat.game_ticks < 100)
    return 0;

  delta = inl_teams[HOME].weighted_score / divisor -
          inl_teams[AWAY].weighted_score / divisor;

  /* 2.0 is the differential */
  if (delta >= 2.0)
    winning_cont = -1;
  else if (delta <= -2.0)
    winning_cont = 1;
  else
    winning_cont = 0;

  /* CONTINUOUS scoring mode:
     delta > 2.0: win
     delta < 2.0 but planet > 11-8-1: win
     otherwise, tie.
   */

  if (inl_stat.score_mode == 1) {

    if (winning_cont)
      return 2;
    else if (winning_norm)
      return 3;
    else
      return 0;

  }

  /* MOMENTUM scoring mode */
  else if (inl_stat.score_mode == 2) {

    /* game tied, go to OT */
    if (!winning_cont && !winning_norm)
      return 0;

    /* if either team is winning by at least one method, return winner */
    if (winning_cont + winning_norm)
      return 4;

    /* otherwise, EXTRA TIME */
    else
      return -1;

  }
  return 0;
}

int end_tourney()
{
  int game_over = 0;
  int win_cond;

  /* check_winner always returns > 0 if winner exists */
  if ((win_cond = check_winner()) > 0)
    {
      pmessage(0, MALL, inl_from, "---------- Game Over ----------");

      /* need to write this to the inl_log because the pmessages don't
         get flushed out to the log before the close */
      fprintf(inl_log, "---------- Game Over ---------\n");

      announce_scores(0, MALL, inl_log);

      switch(win_cond) {
        case 1:
          pmessage(0, MALL, inl_from, "Victory by planet count");
          fprintf(inl_log, "SCORE: Victory by planet count\n");
          break;
        case 2:
          pmessage(0, MALL, inl_from, "Victory by continuous score >= 2.0");
          fprintf(inl_log, "SCORE: Victory by continuous score >= 2.0\n");
          break;
        case 3:
          pmessage(0, MALL, inl_from, "Victory by planet count (continuous score < 2.0)");
          fprintf(inl_log, "SCORE: Victory by planet count (continuous score < 2.0)\n");
          break;
        case 4:
          pmessage(0, MALL, inl_from, "Victory by momentum score");
          fprintf(inl_log, "SCORE: Victory by momentum score\n");
          break;
        default:
          pmessage(0, MALL, inl_from, "Victory by UNKNOWN");
          fprintf(inl_log, "SCORE: Victory by UNKNOWN\n");
          break;
      }

      game_over = 1;
    }


  /* still in regulation, but momentum scoring dictates EXTRA TIME */
  else if ((inl_stat.flags & S_TOURNEY) && (win_cond == -1))
    {
      int extratime = 5 * PERMIN;
      int extra_max = 5 * PERMIN * 3;

      /* only allow 3 cycles of extra time */
      if (inl_stat.extratime < extra_max) {

        pmessage(0, MALL, inl_from, "---------- Extra Time (Momentum) ----------");
        pmessage(0, MALL, inl_from, "%i of %i total minutes added to regulation.",
                 extratime / PERMIN, extra_max);

        fprintf(inl_log, "---------- Extra Time (Momentum) ----------\n");
        fprintf(inl_log, "%i of %i total minutes added to regulation\n",
                extratime / PERMIN, extra_max);

        /* first extra time cycle results in obliteration */
        if (inl_stat.extratime == 0)
          obliterate(0,KPROVIDENCE);

        inl_stat.extratime += extratime;

        inl_game.idx = 0;
        inl_game.end += extratime;
        inl_game.message = "%i %s left in EXTRA time (momentum)";

        announce_scores(0, MALL, inl_log);

      }

      /* after extra time is over, declare continuous score the winner */
      else {
        pmessage(0, MALL, inl_from, "---------- Game Over (Momentum) ----------");

        fprintf(inl_log, "---------- Game Over (Momentum) ----------\n");

        announce_scores(0, MALL, inl_log);

        if (inl_teams[0].weighted_score > inl_teams[1].weighted_score) {
          pmessage(0, MALL, inl_from, "Tie breaker: %s wins by continuous score",
                   sides[inl_teams[0].side_index].name);
          fprintf(inl_log, "Tie breaker: %s wins by continuous score\n",
                  sides[inl_teams[0].side_index].name);
        }
        else {
          pmessage(0, MALL, inl_from, "Tie breaker: %s wins by continuous score",
                   sides[inl_teams[1].side_index].name);
          fprintf(inl_log, "Tie breaker: %s wins by continuous score\n",
                  sides[inl_teams[1].side_index].name);
        }

        game_over = 1;
      }

    }

  else if (inl_stat.flags & S_TOURNEY)
    {
      inl_stat.flags &= ~(S_TOURNEY | S_COUNTDOWN);
      inl_stat.flags |= S_OVERTIME;
      pmessage(0, MALL, inl_from, "---------- Overtime ----------");
      fprintf(inl_log, "---------- Overtime ---------\n");
      obliterate(0,KPROVIDENCE);

      inl_game.idx = 0;
      inl_game.end += inl_stat.overtime;
      /*  inl_game.counts[0] = inl_stat.overtime / (PERMIN*2); */
      inl_game.message = "%i %s left in overtime";

      announce_scores(0, MALL, inl_log);
    }
  else if (inl_game.end <= inl_stat.game_ticks)
    {
      pmessage(0, MALL, inl_from,
	       "------ Game ran out of time without a winner ------");

      fprintf(inl_log, "---------- Game Over (TIED) ---------\n");

      announce_scores(0, MALL, inl_log);
      game_over = 1;
    }

  /* run the stats script for an ending game */
  if (game_over) {
    FILE *fp;
    char pipe[256];
    char name[64];
    struct timeval tv;
    int c, official = 0;

    /* Set the queues so players go to default side */
    queues[QU_HOME].tournmask = HOMETEAM;
    queues[QU_AWAY].tournmask = AWAYTEAM;
    queues[QU_HOME_OBS].tournmask = HOMETEAM;
    queues[QU_AWAY_OBS].tournmask = AWAYTEAM;

    /* make every planet visible */
    for (c = 0; c < MAXPLANETS; c++)
      {
        planets[c].pl_info = ALLTEAM;
      }

    /* and make everybody re-join */
    obliterate(1, TOURNEND);

    status->tourn = 0;

    /* Tourn off t-mode */
    status->gameup |= (GU_CHAOS | GU_PRACTICE);
    status->gameup &= ~(GU_PAUSED & GU_INL_DRAFTED);

    gettimeofday(&tv, (struct timezone *) 0);
    fprintf(inl_log, "TIME: Game ending at %d seconds\n", (int) tv.tv_sec);
    fclose(inl_log);
    blog_printf("inl", "INL robot moderated game ended at %d\n\nLook for your stats at http://www.netrek.org/stats/SERVERNAME/%d/\n", (int) tv.tv_sec, (int) tv.tv_sec);

    sleep(2); /* a kluge to allow time for all the ntservs to run */
              /* savestats() before stats-post processing I hope  */

    /* change all player offset position in playerfile to -1 so that
       stats don't get saved to next game's playerfile.  this is also
       done in reset_inl(1), but the delay causes synchronization
       problems so we do it again here.  this is ugly and should
       probably be fixed.
       CAVEAT: player must logout and login to play a new game.  -da */

    for (c=0; c<MAXPLAYER; c++)
      players[c].p_pos = -1;

    
    sprintf(name, "%s.%d", N_INLLOG, (int) tv.tv_sec);
    if (rename(N_INLLOG, name) != 0)
      ERROR(1,("Rename of INL log failed.\n"));

    if ((inl_log = fopen(N_INLLOG,"w+"))==NULL) {
      ERROR(1,("Could not re-open INL log file.\n"));
      exit(1);
    }

    sprintf(name, "%s.%d", PlayerFile, (int) tv.tv_sec);
    if (rename(PlayerFile, name) != 0)
      ERROR(1,("Rename of player file failed.\n"));

#ifdef PLAYER_INDEX
    sprintf(name, "%s.index", PlayerFile);
    if (unlink(name) != 0)
      ERROR(1,("Unlink of player index file failed.\n"));
#endif

    sprintf(name, "%s.%d", PlFile, (int) tv.tv_sec);
    if (rename(PlFile, name) != 0)
      ERROR(1,("Rename of planet file failed.\n"));

    sprintf(name, "%s.%d", Global, (int) tv.tv_sec);
    if (rename(Global, name) != 0)
      ERROR(1,("Rename of global file failed.\n"));

    /* Stop cambot. */
    if (cambot_pid > 0) {
        kill(cambot_pid, SIGTERM);
        waitpid(cambot_pid, NULL, 0);
        cambot_pid = 0;
        sprintf(name, "%s.%d", Cambot_out, (int) tv.tv_sec);
        if (rename(Cambot_out, name) != 0)
            ERROR(1,("Rename of cambot file failed.\n"));
    }

    for (c=0; c < INLTEAM; c++) {
      if (inl_teams[c].flags & T_REGISTER) official++;
    }

    if (official == 2) {
      for (c=0; c < INLTEAM; c++) {
	int who = inl_teams[c].captain;
	pmessage(who, MINDIV, addr_mess(who, MINDIV),
		 "Official registration script starting.");
      }
      sprintf(pipe, "./end_tourney.pl -register %d", (int) tv.tv_sec);
    } else {
      sprintf(pipe, "./end_tourney.pl -practice %d", (int) tv.tv_sec);
    }

    fp = popen(pipe, "r");
    if (fp == NULL) {
      perror ("popen");
      pmessage(0, MALL|MCONQ, inl_from, "Stats: Cannot popen() %s", pipe);
    } else {
      char buffer[128], *line;

      line = fgets (buffer, 128, fp);
      while (line != NULL) {
	line[strlen(line)-1] = '\0';
	lmessage(line);
	line = fgets (buffer, 128, fp);
      }
      pclose (fp);
      if (official == 2) {
	for (c=0; c < INLTEAM; c++) {
	  int who = inl_teams[c].captain;
	  pmessage(who, MINDIV, addr_mess(who, MINDIV),
		   "Official registration script finished.");
	}
      }
    }

    reset_inl(1);

  } /* if (game_over) */
  return 0;
}

void reset_inl(int is_end_tourney)
     /* is_end_tourney: boolean, used so that the galaxy isn't reset
	at the end of a tournament. */
{
  int c;

  /* Flushing messages */
  checkmess();

  /* Tourn off t-mode */
  status->gameup |= (GU_CHAOS | GU_PRACTICE);
  status->gameup &= ~(GU_PAUSED);

  /* obliterate is taken care of by the calling function for end_tourney */
  if (!is_end_tourney)
      obliterate(1, KPROVIDENCE);
  
  inl_stat.ticks = 0;

  for (c=0; c < INLTEAM; c++) {
    inl_teams[c].flags = 0;		/* reset all flags */
  }

  inl_stat.start_armies = 12;
  inl_stat.change = 1;
  inl_stat.flags = S_PREGAME;
  inl_stat.ticks = 0;
  inl_stat.game_ticks = 0;
  inl_stat.tmout_ticks = 0;
  inl_stat.time = INL_REGULATION * PERMIN;
  inl_stat.overtime = INL_OVERTIME * PERMIN;
  inl_stat.remaining = 0;
  inl_stat.score_mode = 0;
  inl_stat.weighted_divisor = 0.0;

  inl_teams[HOME].team = HOME;
  inl_teams[HOME].name = "home";
  inl_teams[HOME].t_name = "Home";
  inl_teams[HOME].captain = NONE;
  inl_teams[HOME].side = HOMETEAM;
  inl_teams[HOME].side_index = NOT_CHOOSEN;
  inl_teams[HOME].start_armies = 12;
  inl_teams[HOME].time = INL_REGULATION;
  inl_teams[HOME].overtime = INL_OVERTIME;
  inl_teams[HOME].flags = 0;
  inl_teams[HOME].tmout = 1;
  inl_teams[HOME].planets = 10;
  inl_teams[HOME].score_mode = 0;
  inl_teams[HOME].abs_score = 0;
  inl_teams[HOME].semi_score = 0;
  inl_teams[HOME].weighted_score = 0.0;

  inl_teams[AWAY].team = AWAY;
  inl_teams[AWAY].name = "away";
  inl_teams[AWAY].t_name = "Away";
  inl_teams[AWAY].captain = NONE;
  inl_teams[AWAY].side = AWAYTEAM;
  inl_teams[AWAY].side_index = NOT_CHOOSEN;
  inl_teams[AWAY].start_armies = 12;
  inl_teams[AWAY].time = INL_REGULATION;
  inl_teams[AWAY].overtime = INL_OVERTIME;
  inl_teams[AWAY].flags = 0;
  inl_teams[AWAY].tmout = 1;
  inl_teams[AWAY].planets = 10;
  inl_teams[AWAY].score_mode = 0;
  inl_teams[AWAY].abs_score = 0;
  inl_teams[AWAY].semi_score = 0;
  inl_teams[AWAY].weighted_score = 0.0;

  /* Set player queues back to standard starting teams */
  if (is_end_tourney) {
    queues[QU_HOME].tournmask = FED;
    queues[QU_AWAY].tournmask = ROM;
    queues[QU_HOME_OBS].tournmask = FED;
    queues[QU_AWAY_OBS].tournmask = ROM;

    /* change all player offset position in playerfile to -1 so that
       stats don't get saved to next game's playerfile.
       CAVEAT: player must logout and login to play a new game.  -da */

    for (c=0; c<MAXPLAYER; c++)
      players[c].p_pos = -1;

    
  }

  if (!is_end_tourney) {
    pl_reset_inl(1);
  }

}

void init_server()
{
  int i;

  /* Tell other processes a game robot is running */
  status->gameup |= GU_INROBOT;

#ifdef nodef
  /* Fix planets */
  oldplanets = (struct planet *) malloc(sizeof(struct planet) * MAXPLANETS);
  memcpy(oldplanets, planets, sizeof(struct planet) * MAXPLANETS);
#endif

  /* Dont change the pickup queues around - just close them. */
#ifdef nodef
  /* Split the Waitqueues */
  /* Change the regular port to an observer port */
  queues[QU_PICKUP].tournmask	  = (HOMETEAM | AWAYTEAM);
#endif

  /* Nah, just close them down.. */
  queues[QU_PICKUP].q_flags	  &= ~(QU_OPEN);
  queues[QU_PICKUP_OBS].q_flags	  &= ~(QU_OPEN);

  /* these already set in queue.c - assume they are correct */
#ifdef nodef
  /* Ensure some inl queue initialization */
  queues[QU_HOME].max_slots  = TEAMSIZE;
  queues[QU_HOME].tournmask  = HOMETEAM;
  queues[QU_AWAY].max_slots  = TEAMSIZE;
  queues[QU_AWAY].tournmask  = AWAYTEAM;
  queues[QU_HOME_OBS].max_slots	 = TESTERS;
  queues[QU_HOME_OBS].tournmask	 = HOMETEAM;
  queues[QU_HOME_OBS].low_slot	 = MAXPLAYER-TESTERS;
  queues[QU_AWAY_OBS].max_slots	 = TESTERS;
  queues[QU_AWAY_OBS].tournmask	 = AWAYTEAM;
  queues[QU_AWAY_OBS].low_slot	 = MAXPLAYER-TESTERS;
#endif

  /* Open INL ports */
  queues[QU_HOME].free_slots = queues[QU_HOME].max_slots;
  queues[QU_AWAY].free_slots = queues[QU_AWAY].max_slots;
  queues[QU_HOME_OBS].free_slots = queues[QU_HOME_OBS].max_slots;
  queues[QU_AWAY_OBS].free_slots = queues[QU_AWAY_OBS].max_slots;

  status->tourn = 0;

  obliterate(1, KPROVIDENCE);

  /* If players are in game select teams alternately */
  for (i = 0; i<(MAXPLAYER-TESTERS); i++) {
    /* Let's deactivate the pickup slots and move pickup observers
       to the INL observer slots */
#ifdef nodef
    /* Don't change any pickup observers */
    if (players[i].p_status == POBSERV) continue;
#endif
    if (players[i].p_status != PFREE)
      {
	queues[players[i].w_queue].free_slots++;

	if (i%2)
	  players[i].w_queue = (players[i].p_status == POBSERV)
	    ? QU_HOME_OBS : QU_HOME;
	else
	  players[i].w_queue = (players[i].p_status == POBSERV)
	    ? QU_AWAY_OBS : QU_AWAY;

	queues[players[i].w_queue].free_slots--;
      }
  }

  /* dont set all the info - just what is neccessary */
  queues[QU_PICKUP].free_slots = 0;
  queues[QU_PICKUP_OBS].free_slots = 0;
  queues[QU_HOME].q_flags   |= QU_OPEN;
  queues[QU_AWAY].q_flags   |= QU_OPEN;
  queues[QU_HOME_OBS].q_flags	|= QU_OPEN;
  queues[QU_AWAY_OBS].q_flags	|= QU_OPEN;
#ifdef nodef
  queues[QU_PICKUP].low_slot = MAXPLAYER-TESTERS;
  queues[QU_PICKUP].max_slots = TESTERS;
  queues[QU_PICKUP].high_slot = MAXPLAYER;
#endif
}


void
cleanup()
{
  register struct player *j;
  register int i;

  status->gameup &= ~(GU_CHAOS | GU_PRACTICE);
  status->gameup &= ~(GU_PAUSED);

#ifdef nodef
  /* restore galaxy */
  memcpy(planets, oldplanets, sizeof(struct planet) * MAXPLANETS);
#endif

  /* Dont mess with the queue information - it is set correctly in queue.c */
#ifdef nodef
  /* restore old wait queue */
  queues[QU_PICKUP].tournmask=ALLTEAM;
#endif

  /* Open Pickup ports */
  queues[QU_PICKUP].free_slots = queues[QU_PICKUP].max_slots;
  queues[QU_PICKUP_OBS].free_slots = queues[QU_PICKUP_OBS].max_slots;

  /* Close INL ports */
  queues[QU_HOME].q_flags &= ~(QU_OPEN);
  queues[QU_AWAY].q_flags &= ~(QU_OPEN);
  queues[QU_HOME_OBS].q_flags &= ~(QU_OPEN);
  queues[QU_AWAY_OBS].q_flags &= ~(QU_OPEN);
  /* Set player queues back to standard starting teams */
  queues[QU_HOME_OBS].tournmask = HOMETEAM;
  queues[QU_AWAY_OBS].tournmask = AWAYTEAM;

  for (i = 0, j = &players[i]; i < MAXPLAYER; i++)
    {
      j = &players[i];
      if (j->p_status != PFREE)
	{
	  queues[j->w_queue].free_slots++;
	  if (players[i].p_status == POBSERV)
	    j->w_queue = QU_PICKUP_OBS;
	  else
	    j->w_queue = QU_PICKUP;
	  queues[j->w_queue].free_slots--;
	}
      if (j->p_status != PALIVE) continue;
      getship(&(j->p_ship), j->p_ship.s_type);
    }

  queues[QU_HOME].free_slots=0;
  queues[QU_AWAY].free_slots=0;
  queues[QU_HOME_OBS].free_slots=0;
  queues[QU_AWAY_OBS].free_slots=0;

  queues[QU_PICKUP].q_flags |= QU_OPEN;
  queues[QU_PICKUP_OBS].q_flags |= QU_OPEN;

#ifdef nodef
  queues[QU_PICKUP].free_slots += MAXPLAYER-TESTERS;
  queues[QU_PICKUP].low_slot = 0;
  queues[QU_PICKUP].high_slot = MAXPLAYER-TESTERS;
  queues[QU_PICKUP].max_slots = MAXPLAYER-TESTERS;
#endif

  /* Inform other processes that a game robot is no longer running */
  status->gameup &= ~(GU_INROBOT);

  pmessage(0, MALL, inl_from, "##########################");
  pmessage(0, MALL, inl_from, "#  The inl robot has left");
  pmessage(0, MALL, inl_from, "#  INL game is now over. ");
  pmessage(0, MALL, inl_from, "##########################");

  /* Flushing messages */
  checkmess();
  fclose(inl_log);
  exit(0);
}

void start_countdown()
{
  inl_stat.change = 0;

  inl_teams[HOME].side = sides[inl_teams[HOME].side_index].flag;
  inl_teams[AWAY].side = sides[inl_teams[AWAY].side_index].flag;

  /* Set the queues so players go to correct side */
  queues[QU_HOME].tournmask = inl_teams[HOME].side;
  queues[QU_AWAY].tournmask = inl_teams[AWAY].side;
  queues[QU_HOME_OBS].tournmask = inl_teams[HOME].side;
  queues[QU_AWAY_OBS].tournmask = inl_teams[AWAY].side;

  /* Nah.. just deactivate pickup queus.. */
#ifdef nodef
  queues[QU_PICKUP].tournmask = inl_teams[HOME].side | inl_teams[AWAY].side;
  queues[QU_PICKUP_OBS].tournmask =
    inl_teams[HOME].side | inl_teams[AWAY].side;
#endif

  obliterate(2, KPROVIDENCE);

  pmessage(0, MALL, inl_from, "Home team is   %s (%s): %s",
	   inl_teams[HOME].t_name,
	   inl_teams[HOME].name,
	   sides[inl_teams[HOME].side_index].name);
  pmessage(0, MALL, inl_from, "Away team is   %s (%s): %s",
	   inl_teams[AWAY].t_name,
	   inl_teams[AWAY].name,
	   sides[inl_teams[AWAY].side_index].name);
  pmessage(0, MALL, inl_from, " ");
  pmessage(0, MALL, inl_from, "Teams chosen.  Game will start in 1 minute.");
  pmessage(0, MALL, inl_from,
	   "----------- Game will start in 1 minute -------------");
  pmessage(0, MALL, inl_from,
	   "----------- Game will start in 1 minute -------------");

  inl_stat.flags = S_COUNTDOWN;
  inl_countdown.idx = 0;
  inl_countdown.end = inl_stat.ticks+INLSTARTFUSE;
  inl_countdown.action = start_tourney;
  inl_countdown.message = "Game start in %i %s";
}


int start_tourney()
{
  struct timeval tv;
  struct tm *tp;
  char *ap;
  char tmp[MSG_LEN];

  pmessage(0, MALL, inl_from, " ");

  pmessage(0, MALL, inl_from,
	   "INL Tournament starting now. %s(%s) vs. %s(%s).",
	   inl_teams[HOME].t_name,
	   inl_teams[HOME].name,
	   inl_teams[AWAY].t_name,
	   inl_teams[AWAY].name);

  pmessage(0, MALL, inl_from, " ");

  gettimeofday (&tv, (struct timezone *) 0);

  fprintf(inl_log, "TIME: Game started at %d seconds\n", (int) tv.tv_sec);

  tp = localtime (&tv.tv_sec);
  ap = asctime (tp);

  strncpy (tmp, ap + 4, 15);
  *(tmp + 15) = '\0';
  pmessage (0, MALL, inl_from, "%s is the starting time of t-mode play.",
	    tmp);

  switch(inl_stat.score_mode) {
    case 0:
      pmessage(0, MALL, inl_from, "Planet scoring enabled.");
      break;
    case 1:
      pmessage(0, MALL, inl_from, "Continuous scoring enabled.");
      break;
    case 2:
      pmessage(0, MALL, inl_from, "Momentum scoring enabled.");
      break;
  }

  pl_reset_inl(0);

#ifdef nodef
  inl_planets = (struct planet *) malloc(sizeof(struct planet) * MAXPLANETS);
  /*	memcpy(oldplanets, planets, sizeof(struct planet) * MAXPLANETS); */
  memcpy(inl_planets, planets, sizeof(struct planet) * MAXPLANETS);
#endif

  inl_stat.flags |= S_TOURNEY;
  inl_stat.flags &= ~(S_PREGAME | S_COUNTDOWN | S_CONFINE);

  obliterate(1,TOURNSTART);
  reset_stats();

#ifdef nodef
  inl_stat.time = inl_teams[0].time * PERMIN;
  inl_stat.overtime = inl_teams[0].overtime * PERMIN;
#endif
  inl_stat.game_ticks = 0;
  inl_stat.remaining = -1;
  context->inl_game_ticks = inl_stat.game_ticks;
  context->inl_remaining = inl_stat.remaining;

  status->gameup &= ~(GU_CHAOS | GU_PRACTICE);
#ifdef DEBUG
  printf("Setting tourn\n");
#endif
  status->tourn = 1;
  inl_game.idx=0;
  inl_game.end= inl_stat.time;
  /*	inl_game.counts[0] = inl_stat.time / ( PERMIN * 2); */
  inl_game.message = "%i %s left in regulation time";

  /* Start cambot. */
  if (inl_record) {
      int pid;
      pid = fork();
      if (pid < 0)
          perror("fork cambot");
      else if (pid == 0) {
          execl(Cambot, "cambot", (char *) NULL);
          perror("execl cambot");
      }
      else {
          cambot_pid = pid;
      }
  }
  return 0;
}


void obliterate(int wflag, char kreason)
{
  /* 0 = do nothing to war status, 1= make war with all, 2= make peace with all */
  struct player *j;
  int i, k;

  /* set obliterate timer, so we know if it's safe to pause the game */
  obliterate_timer = 10;

  /* clear torps and plasmas out */
  memset(torps, 0, sizeof(struct torp) * MAXPLAYER * (MAXTORP + MAXPLASMA));
  for (j = firstPlayer; j<=lastPlayer; j++) {
    if (j->p_status == PFREE)
      continue;
    if (kreason == TOURNEND)
      j->p_inl_captain = 0;
    if (j->p_status == POBSERV) {
      j->p_status = PEXPLODE;
      j->p_whydead = kreason;
      continue;
    }

    /* sanity checking.  eject players with negative player offset
       positions.  this can happen if there are back-to-back games
       without a server reset.  -da */

    if (!(j->p_flags & PFROBOT) && (j->p_pos < 0)) {

      pmessage(0, MALL, inl_from,
        "** Player %d ejected, must re-login to play.", j->p_no);
      pmessage(j->p_no, MINDIV | MCONQ, addr_mess(j->p_no, MINDIV),
        "You have been ejected due to player DB inconsistency.");
      pmessage(j->p_no, MINDIV | MCONQ, addr_mess(j->p_no, MINDIV),
        "This probably happened because of back-to-back games.");
      pmessage(j->p_no, MINDIV | MCONQ, addr_mess(j->p_no, MINDIV),
               "You must re-login to play.");

      j->p_status = PEXPLODE;
      j->p_whydead = KQUIT;
      j->p_explode = 10;
      j->p_whodead = 0;

      continue;

    }

    j->p_status = PEXPLODE;
    j->p_whydead = kreason;
    if (j->p_ship.s_type == STARBASE)
      j->p_explode = 2 * SBEXPVIEWS ;
    else
      j->p_explode = 10 ;
    j->p_ntorp = 0;
    j->p_nplasmatorp = 0;
    if (wflag == 1)
      j->p_hostile = (FED | ROM | ORI | KLI);	      /* angry */
    else if (wflag == 2)
      j->p_hostile = 0;	      /* otherwise make all peaceful */
    j->p_war = (j->p_swar | j->p_hostile);
    /* all armies in flight to be dropped on home (or alternate) planet */
    if (j->p_armies > 0) {
      k = 10 * (remap[j->p_team] - 1);
      if (k >= 0 && k <= 30) {
	for (i = 0; i < 10; i++) {
	  struct planet *pl = &planets[i+k];
	  if (pl->pl_owner == j->p_team) {
	    if (status->tourn) {
	      char addr_str[9] = "WRN->\0\0\0";
	      strncpy(&addr_str[5], j->p_mapchars, 3);
	      pmessage(0, 0, addr_str,
		       "ARMYTRACK %s {%s} (%d) beamed down %d armies at %s (%d) [%s]",
		       j->p_mapchars, shiptypes[j->p_ship.s_type],
		       j->p_armies, j->p_armies, 
		       pl->pl_name, pl->pl_armies, teamshort[pl->pl_owner]);
	      pl->pl_armies += j->p_armies;
	      j->p_armies = 0;
	    }
	    break;
	  }
	}
      }
    }
  }
}


void countdown(int counter, Inl_countdown *cnt)
{
  int i = 0;
  int j = 0;

  if (cnt->end - cnt->counts[cnt->idx]*PERSEC > counter)
    return;

  while ((cnt->idx != cnt->act) &&
	 (cnt->end - cnt->counts[cnt->idx+1]*PERSEC <= counter))
    {
      cnt->idx++;
    }

  i = cnt->counts[cnt->idx];

  if (i > 60)
    {
      i = i / 60;
      j = 2;
    }
  if (i == 1)
    j++;

  pmessage(0, MALL, inl_from, cnt->message, i, time_msg[j]);
  if (cnt->idx++ == cnt->act) (*(cnt->action))();

}

void pl_reset_inl(int startup)
{
  int i, j, k, which;

  /* this is all over the fucking place :-) */
  for (i = 0; i <= MAXTEAM; i++)
    {
      teams[i].te_turns = 0;
    }
  if (startup)
    {
      memcpy(planets, pl_virgin(), pl_virgin_size());

      for (i = 0; i < MAXPLANETS; i++)
	{
	  planets[i].pl_info = ALLTEAM;
	}

      for (i = 0; i < 4; i++)
	{
	  /* one core AGRI */
	  planets[core_planets[i][random () % 4]].pl_flags |= PLAGRI;

	  /* one front AGRI */
	  which = random () % 2;
	  if (which)
	    {
	      j = random () % 2;
	      planets[front_planets[i][j]].pl_flags |= PLAGRI;

	      /* give fuel to planet next to agri (hde) */
	      planets[front_planets[i][!j]].pl_flags |= PLFUEL;

	      /* place one repair on the other front */
	      planets[front_planets[i][(random () % 3) + 2]].pl_flags |= PLREPAIR;

	      /* place 2 FUEL on the other front */
	      for (j = 0; j < 2; j++)
		{
		  do
		    {
		      k = random () % 3;
		    }
		  while (planets[front_planets[i][k + 2]].pl_flags & PLFUEL);
		  planets[front_planets[i][k + 2]].pl_flags |= PLFUEL;
		}
	    }
	  else
	    {
	      j = random () % 2;
	      planets[front_planets[i][j + 3]].pl_flags |= PLAGRI;
	      /* give fuel to planet next to agri (hde) */
	      planets[front_planets[i][(!j) + 3]].pl_flags |= PLFUEL;

	      /* place one repair on the other front */
	      planets[front_planets[i][random () % 3]].pl_flags |= PLREPAIR;

	      /* place 2 FUEL on the other front */
	      for (j = 0; j < 2; j++)
		{
		  do
		    {
		      k = random () % 3;
		    }
		  while (planets[front_planets[i][k]].pl_flags & PLFUEL);
		  planets[front_planets[i][k]].pl_flags |= PLFUEL;
		}
	    }

	  /* drop one more repair in the core
	     (home + 1 front + 1 core = 3 Repair) */

	  planets[core_planets[i][random () % 4]].pl_flags |= PLREPAIR;

	  /* now we need to put down 2 fuel (home + 2 front + 2 = 5 fuel) */

	  for (j = 0; j < 2; j++)
	    {
	      do
		{
		  k = random () % 4;
		}
	      while (planets[core_planets[i][k]].pl_flags & PLFUEL);
	      planets[core_planets[i][k]].pl_flags |= PLFUEL;
	    }
	}
    }
  else
    {
      struct planet *pdata = pl_virgin();
      for (i = 0; i < MAXPLANETS; i++)
	{
	  planets[i].pl_info = pdata[i].pl_info;
	  planets[i].pl_owner = pdata[i].pl_owner;
	  planets[i].pl_armies = inl_stat.start_armies;
	}
    }
}

void reset_stats()
{
  int i;
  int rank = 0;
  struct player *j;

  /* Reset global stats. */
  /* Taken from main() in daemon.c */
  status->time = 10;
  status->timeprod = 10;
  status->planets = 10;
  status->armsbomb = 10;
  status->kills = 10;
  status->losses = 10;

  /* Reset player stats. */
  for (i = 0; i < MAXPLAYER; i++) {
    j = &players[i];

    /* initial player state as given in getname */
    if (status->gameup & GU_INL_DRAFTED)
      rank = j->p_stats.st_rank;
    memset(&(j->p_stats), 0, sizeof(struct stats));
#ifdef LTD_STATS
    ltd_reset(j);
#else
    j->p_stats.st_tticks = 1;
#endif
    j->p_stats.st_flags=ST_INITIAL;

    if (status->gameup & GU_INL_DRAFTED)
      j->p_stats.st_rank = rank;

    /* reset stats which are not in stats */
    j->p_kills = 0;
    j->p_armies = 0;
    j->p_genoplanets = 0;
    j->p_genoarmsbomb = 0;
    j->p_planets = 0;
    j->p_armsbomb = 0;
  }
}

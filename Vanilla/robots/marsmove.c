/* Marsmove.c
*/

/* Known bugs: 
   get teleported when joining (part of reset_player) 
   -- annoying, but noncritical, i.e. haven't bothered to fix it ;) 
*/

/*
  Based on puckmove.c of 2.01pl5 on 1-7-93 with major modifications
  by Jeff Nelson (jn)

  Modified for use with guzzler code by Michael Kantner (MK)
  Base upon code ftp'd from hobbes.ksu.ksu.edu on 9-9-93
  Please mail all bugs, patches, comments to kantner@hot.caltech.edu

  Updated extensively

  Credits to Terence Chang for writing the initial version of puck
          to Brick Verser for maintaining and improving it
          to numerous others for comments and criticism
*/

#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "roboshar.h"
#include "marsdefs.h"
#include "util.h"
#include "slotmaint.h"


#undef DOGDEBUG              /* debugging compiler option, too much */

#define WEAKER_STR 2		/* integer factor to divide tractstr down */
#define WEAKER_RNG 1.25		/* factor to divide tractrng down */

#define SIZEOF(s)		(sizeof (s) / sizeof (*(s)))
#define AVOID_TIME		4
#define AVOID_CLICKS		200
#define WARFUSE                 5    /* seconds to wait to update war */
#define NORMALIZE(d) 		(((d) + 256) % 256)
#define RANKER(p)		(p->kills + (p->oggs / 2) + (p->mutuals / 4))

#define PU_SITOUT		0x0001
#define PU_FILLER		0x0002

#define PU_DOG			0x1000
#define PU_CONTEST		0x2000
#define PU_WAIT			0x4000
#define PU_DONE			0x8000
#define PU_NOTCONTEST		(PU_SITOUT|PU_DONE|PU_FILLER)  
#define PU_NOTAVAIL		(PU_NOTCONTEST|PU_DOG|PU_WAIT)  

typedef struct DogStat {        /* player record saved to file */
    int d_mutuals;              /* number of tournament mutuals */
    int d_oggs;                 /* number of tournament oggs */
    int d_wins;                 /* number of tournament wins */
    int d_losses;               /* number of tournament losses */
    int d_rating;               /* overall rating */
    int d_timerating;           /* time rating compared to peers */
    int d_ticks;
    int d_pad3;                 /* future expansion */
} DogStat;


typedef struct Track {		/* our record of this player */
    int t_no;                   /* player number */
    int t_x;			/* position at last update */
    int t_y;
    int t_pos;			/* position in player stat file */
    DogStat t_stats;            /* tournament stats structure */
    int t_status;		/* status at last update */
    u_int t_seconds;		/* time in tournament */
    u_int t_flags;		/* MK - new, additional flags */
    char t_arena;		/* arena player is in (0 middle, 1-8 fighting */
    char t_opponent;		/* opponent in dogfight mode */
    float t_score;		/* pretty much an echo of j->p_kills */
    int t_wait;			/* mutual time counter after killing opponent*/
    int t_preferred;		/* player's preferred ship */
    int t_mutuals[MAXPLAYER];	/* mutuals a player has received in tournament*/
    int t_oggs[MAXPLAYER];	/* oggs (worst stat to have, you died first */
    int t_wins[MAXPLAYER];	/* wins with survival */
} Track;


typedef struct DogStatEntry {
    char name[NAME_LEN];
    DogStat dstats;
} DogStatEntry;


typedef struct Arena {          /* record of Arena */
  int a_no;                     /* arena number */
  int a_free;                   /* is arena free? */
  int a_x;                      /* center point of arena */
  int a_y;
  int a_right;                  /* right limit */
  int a_left;                   /* left limit */
  int a_top;                    /* top limit */
  int a_bottom;                 /* bottom limit */
} Arena;

/* Mars specific variables grouped here (mostly) */
Arena arenas[MAXARENAS];
int inprogress = 0;
int toolate2join = 0;
int matchnum = 0;

/*
  The following is included for the sole purpose of making sure
  someone without the minor server mods might just still be
  able to run the robot (without a .sysdef interface though).
*/
#ifndef DOGFIGHT
/* Needed for Mars */
int	nummatch=4;
int	contestsize=2;
int     dogcounts=1;
#endif

Track tracks[MAXPLAYER];

extern char *shiptypes[];
extern char DogStats[256];

/* puck junk */

#define END_OF_PERIOD 20*60*PERSEC
#define PERIODS_PER_GAME 3
#define FACEOFF (30+TELEPORT_DELAY)*PERSEC
#define DEFLECT_DIST 750
#define SHOTRANGE 3000
#define SHOTSPEED 40
#define DEFLECT_SPEED 25	/* ***BAV*** */
#define SHOOTABLE_FACTOR 3
#define DXDISPLACE 8000		/* ***BAV*** was 5000 */
#define DXOFFSET 7000		/* ***BAV*** was 10000 */
#define DYDISPLACE 2000
#define DISPLACE 5000		/* Used for offside and home planet entry */
#define OFFSET 10000
#define SITOUT_X 1000            /* Distance from edge for sitout */

/*
  If someone tries to play more than 256 rounds between each
  player, this will cause trouble.  Not likely. jn
*/

int scores[MAXTEAM+1];
int currp[MAXTEAM+1];
int lastp[MAXTEAM+1];

int faceoff = FACEOFF;		/* "faceoff" mode */
int period = 1;
int roboclock = 1;



extern int debug;
extern int oldmctl;
extern int hostile;
extern int sticky;
extern int practice;
extern int polymorphic;

extern int target;		/* robotII.c 7/27/91 TC */
extern int phrange;		/* robotII.c 7/31/91 TC */
extern int trrange;		/* robotII.c 8/2/91 TC */

extern int length;
extern struct planet *oldplanets;

void start_tourney(void);
void checkstatus(void);
void enter_contest(struct player *j, Track *track);
void udschedule(void);
void checkend(void);
void reset_game(void);
void reportFinal(void);
void reportStanding(void);
int get_num_matches(int a, int b);
void cleanup(void);
int nextFreeArena(void);
void smileon(struct player* j);
void do_msg_check(void);
void do_stats(int who);
void join(int who);
void join_game(Track *track);
int lookupshipname(char *shipname);
void do_score(void);
void player_maint(void);
void enterFight(int a, int b, int ano, int schedule);
void declare_selfhate(struct player *j);
void go_arena(int ano, int pno, int position);
int check_arena_free(int ano);
void recompute_all_kills(void);
void reset_all_players(void);
void reset_player(int pno);
void free_player(int pno);
void reset_dogfight(int pno);
void player_bounce(void);
void exitRobot(void);
void ogg_credit(Track *track, struct player *j, int i);
void win_credit(Track *track, struct player *j, int i);
void mutual_credit(Track *track, struct player *j, int i);
void loss_credit(Track *track, struct player *j, int i);
void savedogplayer(struct player *victim, Track *tr);
void get_dog_stats(struct player *j, Track *track);


void marsmove(void)
{
    /***** Start The Code Here *****/

    me->p_ghostbuster = 0;         /* keep ghostbuster away */

#ifdef DOGDEBUG
    ERROR(2,("Enter marsmove\n"));
#endif

    /* Check that I'm alive */
    /* Messages are not checked when puck is not alive! */

    if (me->p_status != PALIVE){  /*So I'm not alive now...*/
      ERROR(2,("ERROR: Mars died??\n"));
      cleanup();    /*Mars is dead for some unpredicted reason like xsg */
    }

    roboclock++;
    player_maint();   			/* update necessary stats and info */
					/* Moved is up from after mars_rules */

    if (!(roboclock % (PERSEC*2))) do_msg_check();
    if (!(roboclock % (PERSEC*WARFUSE)))
      {
	do_war();
	checkstatus();
      }
    if (!(roboclock % (PERSEC*120))) mars_rules();


    
/* 
  first priority:  is this faceoff mode?

  For the dogfight server, faceoff mode is defined as the
  state where no contests are scheduled to occur, before
  anything has really started.
*/

    if (!inprogress)
      {
#ifdef DOGDEBUG
	ERROR(2,("Exit marsmove 1\n"));
#endif

	return;
      }

    if (faceoff > 0) {
	faceoff--;
	if (faceoff == 0) {
	    start_tourney();
	}
	else if ((faceoff % (10*PERSEC)) == 0) 
	{
	    messAll(255,roboname,"The tournament begins in %d seconds.", faceoff/PERSEC);
	}

        player_bounce();
#ifdef DOGDEBUG
	ERROR(2,("Exit marsmove 2\n"));
#endif
	return;           /* Do nothing else during faceoff mode */
    }



    /* keep all players in the rink */
    /* and do some other bouncing   */
    player_bounce();


    /* Some useful bookkeeping stuff here */
    if (!(roboclock % (PERSEC*120)))
      do_score();
    else if (!(roboclock % (PERSEC*10))) 
      recompute_all_kills();

    if (!(roboclock % (PERSEC*5))) udschedule();

#ifdef DOGDEBUG
      ERROR(2,("Exit marsmove 3\n"));
#endif

    update_sys_defaults();

}  /* End of marsmove, the main dogfight overseeing program */


void start_tourney(void)
{

  int i;
  struct player *j;
  Track *track;

  ERROR(4,("In start_tourney\n"));
  inprogress = 2;
  messAll(255,roboname,"War has broken out.");

  /* make sure these are done in a timely manner */
  do_war();		/* open hostilities */

  /* avoid dead slots, me, other robots (which aren't hostile) */
  for (i = 0, j = &players[i], track = &tracks[i];
       i < MAXPLAYER; i++, j++, track++) 
    {

      if ((j == me) || 
          (j->p_status == PFREE) ||
	  (track->t_flags & PU_FILLER) ||
	  (track->t_flags & PU_SITOUT))    /*ignore puck,filler,watchers*/ 
	continue;

      enter_contest(j,track);
    }

  udschedule();
}

void checkstatus(void)
{

  int i;
  struct player *j;
  Track *track;
  int numplayers = 0;

  /* avoid dead slots, me, other robots (which aren't hostile) */
  for (i = 0, j = &players[i], track = &tracks[i];
       i < MAXPLAYER; i++, j++, track++) 
    {

      if ((j == me) ||
	  (j->p_status == PFREE) ||
	  (track->t_flags & PU_FILLER) ||
	  (track->t_flags & PU_SITOUT))    /*ignore puck,filler,watchers*/ 
	continue;

      numplayers++;
    }

  if (numplayers >= contestsize)
    {
      if (!inprogress)
	{
	  inprogress = 1;
	  faceoff = FACEOFF;
	}
    }
  else if (inprogress)
    {
      messAll(255,roboname,"Too many people have left.  The contest is over.");
      reset_game();
    }
}


void enter_contest(struct player *j, Track *track)
{
  reset_player(j->p_no);

  track->t_flags |= PU_CONTEST;

  if (track->t_pos < 0) get_dog_stats(j,track);
}


void udschedule(void)
{
  register int i,k,n;
  int avail[MAXPLAYER];
  struct player *j;
  Track *track;
  int schedule=0;

  if (inprogress != 2)
    return;

  k = 0;

  /* avoid dead slots, me, other robots (which aren't hostile) */
  for (i = 0, j = &players[i], track = &tracks[i];
       i < MAXPLAYER; i++, j++, track++)
    {

      if ((j == me) || 
	  (j->p_status != PALIVE) ||
	  (track->t_flags & PU_NOTAVAIL))    /*ignore puck,filler,watchers*/ 
	continue;

      avail[k++] = i;
    }

  avail[k]=0;
  if (k && (nextFreeArena()== -1)) 
    {
      messAll(255,roboname,"All arenas are full! Please wait for next available one.");
      return;
    }
  
/*
  This is just a quick gizmo to make sure the guy at the end
  of the line isn't always left waiting.
  Don't know if it's worth the CPU.
*/
#ifdef SCHEDULER
  if (k > 0)
    {
      int jumpsize;
      int tmp;
      int jumpto;

      jumpsize = random() % k;

      if (jumpsize)
	{
	  for (i=0;i<k;i++)
	    {
	      tmp = avail[i];

	      jumpto = (i + jumpsize) % k;

	      avail[i] = avail[jumpto];
	      avail[jumpto] = tmp;
	    }
	}
    }
#endif

  for (i=0;i<k;i++) 
    {
      if (avail[i]== -1) continue;
      for (n=i+1;n<k;n++) {
	if (avail[n]== -1) continue;
	schedule = get_num_matches(avail[i],avail[n]);
	if (schedule < nummatch) {
	  enterFight(avail[i],avail[n],nextFreeArena(),schedule);
	  avail[n]=avail[i]= -1;
	  break;
	}
      }

      if (nextFreeArena()== -1) break;
    }

    checkend();
}


void checkend(void)
{
    register int i,k;
    struct player *p;

    /* First, we check that no one is dogfighting anymore */
    for(i=0,p=&players[0];i<MAXPLAYER;i++,p++) 
      {
	if (p->p_status == PFREE) continue;
	if (p == me) continue;
	if (tracks[i].t_flags & (PU_NOTCONTEST)) continue;
	if (tracks[i].t_flags & (PU_DOG|PU_WAIT)) 
	  {
#ifdef DOGDEBUG
	    ERROR(2,("Returning on %d",i));
#endif
	    return;
	  }
      }

    /* Next, we check that all schedules are played */
    for(i=0;i<MAXPLAYER;i++) 
      {
	if (tracks[i].t_flags & PU_NOTCONTEST) continue;
	if (players[i].p_status==PFREE) continue;
	if (i == me->p_no) continue;

	for (k=0;k<MAXPLAYER;k++) 
	  {
	    if (k == i) continue;
	    if (tracks[k].t_flags & PU_NOTCONTEST) continue;
	    if (k == me->p_no) continue;
	    if (players[k].p_status==PFREE) continue;

	    if (get_num_matches(i,k) < nummatch)
	      {
#ifdef DOGDEBUG
		ERROR(2,("Returning on %d %d",i,k));
#endif
		return;
	      }
	  }

	if (!toolate2join)
	  {
	    messAll(255,roboname,"No more players will be allowed to join the tournament until completed.");
	    toolate2join = 1;
	  }

	tracks[i].t_flags |= PU_DONE;
	tracks[i].t_flags &= ~PU_CONTEST;
      }

    player_maint();  /* seems to be necessary  -Jon */
    reset_game();
}

void reset_game(void)
{
    /* Ok, now for the results */
  if (inprogress == 2) reportFinal();
    
  messAll(255,roboname,"Resetting game.");
  reset_all_players();
  inprogress = 0;
  toolate2join = 0;
  matchnum = 0;
}

/*  Display complete individual record for every player */
void reportStats(void)
{
  int i;

  for(i=0;i<MAXPLAYER;i++) 
    {
      if (i == me->p_no) continue;
      if (players[i].p_status == PFREE) continue;
      if (tracks[i].t_flags & (PU_FILLER|PU_SITOUT)) continue;

      do_stats(i);
    }
}

void reportFinal(void)
{
    messAll(255,roboname,"   *****   GAME IS COMPLETED!   *****");
    reportStats();
    reportStanding();
}

typedef struct playerlist {
    char name[16];
    char mapchars[2];
    float kills;
} Players;

void reportStanding(void)
{
    register int i,k,l;
    register struct player *j;
    float kills;
    Players winners[MAXPLAYER+1];
    char buf[80],buf2[80];
    int number;


    recompute_all_kills();

    number=0;
    for (i = 0, j = &players[0]; i < MAXPLAYER; i++, j++) {
	if (j->p_status == PFREE) continue;
	if (j == me) continue;
	if (tracks[i].t_flags & PU_SITOUT) continue;

	kills= tracks[i].t_score;
	for (k = 0; k < number; k++) {
	    if (winners[k].kills < kills) {
		/* insert person at location k */
		break;
	    }
	}
	for (l = number; l >= k; l--) {
	    winners[l+1] = winners[l];
	}
	number++;
	winners[k].kills=kills;
	strncpy(winners[k].mapchars, j->p_mapchars, 2);
	strncpy(winners[k].name, j->p_name, 16);
	winners[k].name[15]=0;	/* `Just in case' paranoia */
    }

    messAll(255,roboname,"-----  RANKINGS:  -------");

    strcpy(buf,"");
    for (k=0; k < number; k+=2) {
	for (l=k;l<k+2;l++) {
	    if (l >= number) continue;
	    sprintf(buf2, "#%d:  %16s (%2.2s),%0.2f      ", l+1,
		winners[l].name, winners[l].mapchars,winners[l].kills);
	    strcat(buf,buf2);
	}
	messAll(255,roboname,buf);
	strcpy(buf,"");
    }
    return;
}


int get_num_matches(int a, int b)
{
  int total;

  checkBadPlayerNo(a,"in get_num_matches a\n");
  checkBadPlayerNo(b,"in get_num_matches b\n");

  total = tracks[a].t_wins[b] + tracks[b].t_wins[a]
    + tracks[a].t_mutuals[b] + tracks[a].t_oggs[b];

  return(total);
}

/*  
 *  Additional routine start here
 */

void cleanup(void)
{
    register struct player *j;
    register int i;

    /* Free the held slots - MK 9-20-92 */
    for (i=12, j = &players[i]; i < MAXPLAYER - TESTERS; i++, j++) {
        if ( j->p_status==POUTFIT && j->p_team==NOBODY){
            j->p_status = PFREE;
	}
    }

    if (!practice) {
	memcpy(planets, oldplanets, sizeof(struct planet) * MAXPLANETS);
	for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	    if ((j->p_status != PALIVE) || (j == me)) continue;
	    getship(&(j->p_ship), j->p_ship.s_type);
	}
    }
    status->gameup &= ~GU_DOG;
    exitRobot();
}

void do_teleport_home(int pno)
{
  checkBadPlayerNo(pno, "in teleport_home\n");

  tracks[pno].t_flags &= ~(PU_DOG|PU_WAIT);

  go_arena(0,pno,0);
}


void do_teleport_arena(int ano, struct player* j, int position)
{
    Arena *a;
    Track *track;

    /*Cleaned up this routine for v2 - MK*/

    if (!j)
      {
	ERROR(2,("NULL passed to teleport_arena\n"));
	return;
      }

    if (j == me)
      {
	ERROR(2,("Tried to teleport self.\n"));
	return;
      }

    checkBadArenaNo(ano,"new in teleport\n");
    a = &arenas[ano];
    track = &tracks[j->p_no];


    if (j->p_status == PALIVE)
      {
	int x, y;

	if (ano == 0)
	  {
	    x = planets[STARTPLANET].pl_x + (random() % (2*DISPLACE)-DISPLACE);
	    y = planets[STARTPLANET].pl_y + (random() % (2*DISPLACE)-DISPLACE);
	  }
	else
	  {
#ifdef DOG_RANDOM
	    x = a->a_x + (2 * position - 1) * (10000 + (random() % 4500));
	    y = a->a_y - (2 * position - 1) * (random() % 9000);
#else
	    x = a->a_x + (2 * position - 1) * 9000;
	    y = a->a_y - (2 * position - 1) * 6000;
#endif
	  }
	p_x_y_set(j, x, y);
	
	smileon(j);

	track->t_x = x;
	track->t_y = y;
      }

    track->t_arena = ano;

    a->a_free = 0;
}


int nextFreeArena(void)
{
    register int i;
    Arena* a;

    for(i=1, a = &arenas[1];i<MAXARENAS;i++, a++) {
	if (a->a_free) return i;
    }

    return (-1);
}



/* fully heal this dude */
void smileon(struct player* j)
{
  register struct torp *t;
  j->p_speed = 0;
  j->p_desspeed = 0;
  j->p_subspeed = 0;
  j->p_flags = PFSHIELD;
  j->p_fuel   = j->p_ship.s_maxfuel;
  j->p_shield = j->p_ship.s_maxshield;
  j->p_damage = 0;
  j->p_wtemp = 0;
  j->p_etemp = 0;
  j->p_ntorp = 0;
  for (t = firstTorpOf(j); t <= lastTorpOf(j); t++)
    t->t_status = TFREE;
  j->p_cloakphase = 0;
}


void do_msg_check(void)
{
    while (oldmctl!=mctl->mc_current) {
	oldmctl++;
	if (oldmctl >= MAXMESSAGE || oldmctl < 0) oldmctl=0;
	robohelp(me, oldmctl, roboname);
    }
}



/* Added by MK */
/* ARGSUSED */
void do_version(char *comm, struct message *mess)
{
    int who;

    who = mess->m_from;

    messOne(255,roboname,who, "Mars Version 1.0 - Beta Release Version");
    messOne(255,roboname,who, "Please mail bug/comments to nelson@math.arizona.edu");
}


/* ARGSUSED */
void do_stats_msg(char *comm, struct message *mess)
{
    int who;

    who = mess->m_from;

    do_stats(who);
}

void do_stats(int who)
{
    register int i;
    register Track *track;
    register struct player *j;

    checkBadPlayerNo(who,"in do_stat\n");

    messOne(255,roboname,who,"You must play %d games with each person.",nummatch);
    messOne(255,roboname,who,"You must have %d people to start the game.",contestsize);

    if (inprogress < 2)
      {
        messOne(255,roboname,who,"The tournament has not started yet.");
	return;
      }
    
    messOne(255,roboname,who,"Your record so far:");

    track = &tracks[who];

    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) 
      {
	if (j == me) continue;
	if (tracks[i].t_flags & (PU_FILLER|PU_SITOUT)) continue;
	if (j->p_status == PFREE) continue;
	if (i==who) continue;

	messOne(255,roboname,who, " %2s: %-16s  %2dW %2dL %2dM %2dO  %3d left  %3d min",
	      j->p_mapchars,
	      j->p_name,
	      track->t_wins[i],
	      tracks[i].t_wins[who],
	      track->t_mutuals[i],
	      track->t_oggs[i],
	      nummatch - get_num_matches(i,who),
	      tracks[i].t_seconds / 60);
      
      }
}

/*
  Remove some jerk from the score keeping
*/
void dont_score(int pno)
{
  int i;
  Track *track;

  if (!inprogress)
    return;

  checkBadPlayerNo(pno,"in don't score\n");

  for (i = 0, track = &tracks[i];
       i < MAXPLAYER; i++, track++) 
    {
      /*Skip free slots, space filler, and puck */

      track->t_mutuals[pno] = 0;
      track->t_oggs[pno] = 0;
      track->t_wins[pno] = 0;
    }

  recompute_all_kills();
}

/* ARGSUSED */
void do_force_newgame(int mflags, int sendto)
{

/****MK - stats are cleared*/
#ifdef NEWGAME_RESET

    messAll(255,roboname,"Clearing Player Stats for the Newgame");
    reset_game();

#else
    messAll(255,roboname,"NEWGAME is disabled on this server.");

#endif
/****End added code */

}

void do_war(void)
{
    register int i;
    register struct player *j;

    if (practice) return;
    for (i = 0; i < 40; i++) {	/* ***BAV*** */
	planets[i].pl_armies = 2;
    }
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {

#ifdef DOGFILLER
	if (i>11 && i<MAXPLAYER-TESTERS && j->p_status==PFREE) { /* **BAV** */
	    j->p_status = POUTFIT;  /* Mark a few players slots as busy */
	    j->p_team = NOBODY;
	    j->p_ghostbuster = -2147483000;
            tracks[i].t_flags = PU_FILLER;
	}
#endif

	if ((j->p_status == PALIVE) && (j != me)) 
	  {
	    tracks[i].t_seconds += WARFUSE;

	    declare_selfhate(j);
	  
	    j->p_armies = 0;
	  }
    }
}

/* ARGSUSED */
void do_join(char *comm, struct message *mess)
{
    int who;

    who = mess->m_from;
    join(who);
}

void join(int who)
{ 
  Track *track;

  track = &tracks[who];

/*
  if (!(track->t_flags & PU_FILLER))
	track->t_flags |= PU_FILLER;
*/
  
  /*If you have already joined, do nothing*/
  if (track->t_flags & (PU_CONTEST|PU_DONE))
    {
      messOne(255,roboname,who,"You have already joined.");
      return;
    }

  if (!inprogress || !toolate2join)
    {
      messAll(255,roboname,"Oh, look who's decided to grace us with his presence: %s (%c)", 
	    players[who].p_name, shipnos[who]);

      join_game(track);
    }
  else
    {
      messOne(255,roboname,who,"It is too late to join this game.");
      track->t_flags |= PU_SITOUT;
    }
}

void join_game(Track *track)
{
  
  /*If you are already sitting out, do nothing*/
  track->t_flags = 0;

  reset_player(track->t_no);
  if (inprogress == 2) 
    track->t_flags |= PU_CONTEST;

  if (track->t_pos < 0) get_dog_stats(&players[track->t_no],track);
  /* reset preferred ship; not initialized elsewhere, funny things happened */
  track->t_preferred = BATTLESHIP;
}


/* ARGSUSED */
void do_sitout(char *comm, struct message *mess)
{
    int who;
    struct player *j;
    Track *track;

    who = mess->m_from;
    j = &players[who];
    track = &tracks[who];

    /*If you are already sitting out, do nothing*/
    if (track->t_flags & PU_NOTCONTEST)
      {
	messOne(255,roboname,who,"You are already sitting out");
	return;
      }

    if (inprogress == 2)
      {
#ifdef ALLOWQUIT
	reset_player(who);
	dont_score(who);
#else
	messOne(255,roboname,who,"What, you chicken! Go fight!");
	return;
#endif
      }

    messAll(255,roboname,"%s (%c) has decided to watch, what a twink.", 
	    players[who].p_name, shipnos[who]);

    do_teleport_home(who);

    track->t_flags |= PU_SITOUT ;

    j->p_speed = 0;                      /*So you don't move        */
    j->p_desspeed = 0;
    j->p_subspeed = 0;
    j->p_flags = PFSHIELD;               
    j->p_fuel   = 0;                     /*Hah, you're hosed anyway */
    j->p_damage = j->p_ship.s_maxdamage - 5;
    j->p_shield = j->p_ship.s_maxshield; /*OK, a potential help     */

}


void mars_rules(void)
{
    messAll(255,roboname,"Welcome to the Dogfight Challenge.");
    messAll(255,roboname,"Send me the message \"help\" for rules.");
}

void do_preferred_ship(char *comm, struct message *mess)
{
    int who;
    char shiptype[2];
    int shipnum;

    who = mess->m_from;

    checkBadPlayerNo(who,"in do_prefered_ship");

    sscanf(comm,"SHIP %2s",shiptype);

    shipnum = lookupshipname(shiptype);

    /*
      The default is no starbases or ATTs
   */
    if (shipnum >= 0 && shipnum < ALLOWED_SHIPS) {
	tracks[who].t_preferred = shipnum;
	messOne(255,roboname,who,"Preferred ship type set to: %s",shipnames[shipnum]);
    }
    else {
	messOne(255,roboname,who,"I don't recognize that type of ship.");
	messOne(255,roboname,who,"How about BB, CA, DD, or SC?");
    }
}

int lookupshipname(char *shipname)
{
    int i;

    for (i = 0; i < NUM_TYPES; i++) {
	if (strncasecmp(shipname,shiptypes[i],2) == 0)
	return(i);
	if (strcasecmp(shipname,shipnames[i]) == 0)
	return(i);
    }

    return (-1);
}

void do_score(void)
{
  if (inprogress==2)
    reportStanding();
}

/* ARGSUSED */
void do_score_msg(char *comm, struct message *mess)
{
    int who;

    who = mess->m_from;

    if (inprogress==2)
	reportStanding();
    else
	messOne(255,roboname,who,"The tournament has not yet started.");
}

/* ARGSUSED */
void do_status_msg(char *comm, struct message *mess)
{
    int who;

    who = mess->m_from;

    checkBadPlayerNo(who,"in do_status_msg");

    if (!inprogress)
	messOne(255,roboname,who,"The tournament has not yet started.");
    else if (inprogress == 1)
	messOne(255,roboname,who,"Faceoff is occuring now.");
    else
	messOne(255,roboname,who,"The tournament is in progress.");

    if (tracks[who].t_flags & PU_SITOUT)
	messOne(255,roboname,who,"You are sitting out.");
    if (tracks[who].t_flags & PU_CONTEST)
	messOne(255,roboname,who,"You are a contestant.");
    if (tracks[who].t_flags & PU_DONE) {
	messOne(255,roboname,who,"You are done with all dogfights, but you must");
	messOne(255,roboname,who,"remain so scores can be totalled.");
    }
    if (tracks[who].t_flags & PU_DOG)
	messOne(255,roboname,who,"You are currently in a tournament dogfight.");
    if (tracks[who].t_flags & PU_WAIT)
	messOne(255,roboname,who,"You are waiting to receive credit for a kill.");
}


	
void player_maint(void)
{
  int i;
  struct player *j;
  struct player *enemy;
  Track *etrack;
  Track *track;
  int eno;
  
  /* avoid dead slots, me, other robots (which aren't hostile) */
  for (i = 0, j = &players[i], track = &tracks[i];
       i < MAXPLAYER; i++, j++, track++) 
    {
      if (j->p_pos > 0) j->p_pos = -1;		/* Don't save player stats */
 
      if ((j == me) || (track->t_flags == PU_FILLER)) /*ignore puck,filler*/ 
	continue;

      if (j->p_status != PALIVE) {       /*for dead people*/

	j->p_team = ROM;

	if ((j->p_status == PFREE) &&
	    (track->t_status != j->p_status)) 
	  {
	    free_player(i);
	    if (inprogress && (track->t_flags & (PU_CONTEST|PU_DONE)))
	      {
		messAll(255,roboname,"%s (%c) has left the game!!!  Removing his effect on the scores.", 
			j->p_name, shipnos[i]);
		dont_score(i);
	      }
	  }
	else if (track->t_flags & (PU_DOG))
	  {
	    track->t_flags &= ~PU_DOG;

	    /* The player died in a tournament dogfight */
	    
	    if (track->t_flags & PU_WAIT)
	      {
		/*
		  Somehow the enemy killed this player after he died.
		  Consider how to divide credit.
	        */
		
		track->t_flags &= ~PU_WAIT;

		eno = track->t_opponent;
		checkBadPlayerNo(eno,"in maint\n");
		enemy = &players[eno];
		etrack = &tracks[eno];

		if (j->p_whydead == KSHIP)
		  {
		    if (enemy->p_whydead == KSHIP)
		      {
			/* this should not happen, but just in case */
			ogg_credit(track,j,eno);
			ogg_credit(etrack,enemy,i);
		      }
		    else
		      {
			mutual_credit(track,j,eno);
			ogg_credit(etrack,enemy,i);
		      }
		  }
		else
		  {
		    mutual_credit(track,j,eno);
		    mutual_credit(etrack,enemy,i);
		  }

	      }
	    else
	      {
		/*
		  This player was killed first by his enemy.
		  Place enemy in wait mode, credit is decided
		  when fate of enemy is known.
		*/
		
		eno = track->t_opponent;
		checkBadPlayerNo(eno,"in maint\n");
		enemy = &players[eno];
		etrack = &tracks[eno];

		if (!(etrack->t_flags & PU_DOG))
		  {
		    /* 
		      something weird is going on.. enemy not dogfighting, 
		      remove both player and enemy from dogfight mode
		    */
		    messAll(255,roboname,"Something weird is going on with %s (O%c)!!", 
			    j->p_name, shipnos[i]);
		    reset_dogfight(i);
		    continue;
		  }

		etrack->t_flags |= PU_WAIT;
		etrack->t_wait = 6*PERSEC;
	      }	    
	  }

	track->t_opponent = -1;
	track->t_status = j->p_status;
	do_teleport_home(i);

	continue;		/* do nothing else for dead players */
      }
      
      
      /*At this point, the player is alive*/	
      
      if (track->t_flags & PU_DOG)
	{
	  /* increment time in the arena */
#ifndef LTD_STATS
	  j->p_stats.st_tticks = ++(track->t_stats.d_ticks); 
#endif

	  if (track->t_flags & PU_WAIT)
	    {
	      track->t_wait--;

	      if (track->t_wait == 0)
		{
		  /*
		    This player survived long enough past the enemy's death!  
		    He gets full credit!!
		  */
		  track->t_flags &= ~PU_WAIT;

		  eno = track->t_opponent;
		  checkBadPlayerNo(eno,"in maint\n");
		  enemy = &players[eno];
		  etrack = &tracks[eno];

		  win_credit(track,j,eno);
		  loss_credit(etrack,enemy,i);

		  track->t_opponent = -1;
		  track->t_flags &= ~PU_DOG;
		  do_teleport_home(i);
		}
	    }
	  else
	    {
	      eno = track->t_opponent;
	      checkBadPlayerNo(eno,"in maint\n");
	      enemy = &players[eno];
	      etrack = &tracks[eno];

	      if (!(etrack->t_flags & PU_DOG))
		{
		  /*
		    Somethings up... my enemy isn't in dogfight mode...
		  */
		  messAll(255,roboname,"%s (%c) can't find his enemy: %s (%c)", 
			    j->p_name, shipnos[i],
			    enemy->p_name, shipnos[eno]);
		  reset_dogfight(i);
		  continue;
		}
	    }
	}
      else if ((inprogress == 2) &&
	       !(track->t_flags & (PU_SITOUT|PU_DONE|PU_CONTEST)))
	{
	  printf("FLAGS: 0x%x\n",track->t_flags);
	  /* A new victim has arrived. */
	  join(i);
	}

      /* Save current info */
      track->t_status = j->p_status;
      track->t_x = j->p_x;
      track->t_y = j->p_y;
      
      /* Note: track->t_seconds is updated in do_war */
      
    }
}


/*
  go_arena
*/
void enterFight(int a, int b, int ano, int schedule)
{
    /* 1. Removes arena_num from free arena list    D>
       2. Sets p_pflag & PFDOG			    <D>
       3. Announces fight				*/

    char buf[80];
    struct player *p1, *p2;
    int times_fought = 0, shiptype1 = 0, shiptype2 = 0;
    Track *tr1;
    Track *tr2;

    checkBadPlayerNo(a,"in enterFight a\n");
    checkBadPlayerNo(b,"in enterFight b\n");
    checkBadArenaNo(ano,"in enterFight\n");

    p1 = &players[a];
    p2 = &players[b];
    tr1 = &tracks[a];
    tr2 = &tracks[b];

    if (p1 == me || p2 == me) {
	ERROR(2,("Just tried to enter myself in a fight.  Exitting.\n"));
	cleanup();
    }
	
    /* announces match */
    sprintf(buf,"[MATCH %d] %s(%c%c) versus %s(%c%c) in arena %d!",
	    matchnum++,p1->p_name,
	    teamlet[p1->p_team],shipnos[p1->p_no],p2->p_name,
	    teamlet[p2->p_team],shipnos[p2->p_no],ano);
    messAll(255,roboname,buf);
    messOne(255,roboname,a,buf);
    messOne(255,roboname,b,buf);
    
    times_fought = (schedule % 5);
    switch(times_fought) {
	case 0:
	    shiptype1 = shiptype2 = BATTLESHIP;
	    break;
	case 1:
	    shiptype1 = shiptype2 = CRUISER;
	    break;
	case 2:
	    shiptype1 = shiptype2 = DESTROYER;
	    break;
	case 3:
	    shiptype1 = shiptype2 = SCOUT;
	    break;
	case 4:
	    shiptype1 = tr1->t_preferred;
	    shiptype2 = tr2->t_preferred;
	    break;
	default:
	    shiptype1 = shiptype2 = -1;	/* don't change their ship type */
	    break;
    }
    if (shiptype1 != -1)
	getship(&p1->p_ship, shiptype1);
    if (shiptype2 != -1)
	getship(&p2->p_ship, shiptype2);

    tr1->t_opponent = b;
    tr2->t_opponent = a;
    tr1->t_flags |= PU_DOG;
    tr2->t_flags |= PU_DOG;

    go_arena(ano,a,0);
    go_arena(ano,b,1);
}


/* Rule 1)  Love your enemies, hate your friends */
void declare_selfhate(struct player *j)
{
#ifdef NO_BRUTALITY
  if (j->p_team == ROM)
    j->p_hostile &= ~(ALLTEAM);
  else
#endif
    {
      j->p_hostile &= ~(ALLTEAM & ~j->p_team);
      j->p_hostile |= j->p_team;
    }
  j->p_war = (j->p_swar | j->p_hostile);
}

/*
  Remove a playera from whatever arena currently in, 
  place them in the new arena.
  Also heals, switches the player's team, and declares war.
*/
void go_arena(int ano, int pno, int position)
{
  Arena *old_a = NULL;
  Track *track;
  struct player *j;

  checkBadPlayerNo(pno,"in go_arena");

  j = &players[pno];
  smileon(j);

  track = &tracks[j->p_no];
     
  if (badArenaNo(track->t_arena))
    {
      ERROR(2,("Bad old arena number %d for %d.  Resetting player\n",
	       old_a ? (int)(old_a->a_no) : -1, pno));
      reset_player(pno);
      return;
    }

  old_a = &arenas[(int) track->t_arena];


  checkBadArenaNo(ano,"in teleport\n");



  if (ano == 0)
    {
      j->p_team = ROM;
    }
  else
    {
      switch (ano % 3)
	{
	case 0:
	  j->p_team = FED;
	  break;
	case 1:
	  j->p_team = KLI;
	  break;
	default:
	  j->p_team = ORI;
	  break;
	}
    }

  /* Rule 1)  Love your enemies, hate your friends */
  declare_selfhate(j);
  
  do_teleport_arena(ano, j, position);
 
  check_arena_free(old_a->a_no);
}


int check_arena_free(int ano)
{
  int i;
  Track *track;
  struct player *j2;

  
  checkBadArenaNo(ano,"check_arena\n");

  if (ano == 0) return 1;


  for (i = 0, j2 = &players[i], track = &tracks[i];
       i < MAXPLAYER; i++, j2++, track++) 
    {
      if ((j2 == me) || 
	  (j2->p_status!=PALIVE))
	continue;  /*Don't do this for dead and for puck*/

      if (track->t_arena == ano)
	{
	  arenas[ano].a_free = 0;
	  return 0;
	}
    }

  arenas[ano].a_free = 1;
  return 1;
}


void recompute_kills(struct player* j2)
{
  int i;
  Track *track;
  Track *track2;
  struct player *j;
  float kills;


  track2 = &tracks[j2->p_no];
  kills = 0.0;

  for (i = 0, j = &players[i], track = &tracks[i];
       i < MAXPLAYER; i++, j++, track++) {

	if (j == me) continue;
	if (track->t_flags & (PU_FILLER|PU_SITOUT)) continue;
	if (j->p_status == PFREE) continue;
	if (j==j2) continue;

      kills += (float)( (float)(track2->t_wins[i])
	    + (float)OGGVALUE * (float)(track2->t_oggs[i])
	    + (float)MUTUALVALUE * (float)(track2->t_mutuals[i]));
    }

  j2->p_kills = track2->t_score = kills;

  /* this updates the shared memory so the player can see stats */
#ifndef LTD_STATS
  j2->p_stats.st_tkills = track2->t_stats.d_mutuals +
                          track2->t_stats.d_wins + track2->t_stats.d_oggs;
  j2->p_stats.st_tarmsbomb = track2->t_stats.d_wins + track2->t_stats.d_oggs;
  j2->p_stats.st_tplanets = track2->t_stats.d_wins;
  j2->p_stats.st_tlosses = track2->t_stats.d_losses;
#endif
}


/*
  recompute all kills, just to be sure the server didn't
  pull a fast one on us
*/

void recompute_all_kills(void)
{
    register int i;
    register struct player *j;

    for (i = 0, j = &players[i];
	 i < MAXPLAYER; i++, j++) 
      {

	if ((j == me) ||
	    (j->p_status == PFREE))
	  continue;
	
	recompute_kills(j);
      }
}

void reset_all_players(void)
{
    register int i;
    register struct player *j;

    for (i = 0, j = &players[i];
	 i < MAXPLAYER; i++, j++) {
      if ((j->p_status != PFREE) && !(j->p_flags & PFROBOT)) {
	reset_player(j->p_no);
      }
    }
}

void zero_stats(Track *track)
{
    memset(track->t_mutuals, 0, sizeof(track->t_mutuals[0]) * MAXPLAYER);
    memset(track->t_oggs, 0, sizeof(track->t_oggs[0]) * MAXPLAYER);
    memset(track->t_wins, 0, sizeof(track->t_wins[0]) * MAXPLAYER);
/*
    memset(track->t_votes, sizeof(track->t_votes[0]) * PV_TOTAL);
*/
}


/* 
  fully reset this guy

  This throws the player out of the contest
  It is generally appropriate to use dont_score() with this
*/
void reset_player(int pno)
{
  Track *track;

  checkBadPlayerNo(pno,"in reset_player\n");

#ifdef DOGDEBUG
  ERROR(2,("Resetting %d\n",pno));
#endif


  track = &tracks[pno];

  zero_stats(track);

  track->t_opponent   = -1;
  track->t_wait   = 0;

  if ((inprogress == 2) && (track->t_flags & PU_CONTEST))
    track->t_flags   = PU_CONTEST;
  else if (track->t_flags & PU_SITOUT)
    track->t_flags   = PU_SITOUT;
  else
    track->t_flags   = 0;

  do_teleport_home(pno);

  track->t_score = 0.0;
  track->t_seconds = 0;
}

/* free player so someone else can use this slot */
void free_player(int pno)
{
  Track *track;

  checkBadPlayerNo(pno,"in reset_player\n");

  ERROR(3,("Freeing player %d\n",pno));

  track = &tracks[pno];

  zero_stats(track);

  track->t_score = 0.0;
/*		- nbt
  track->t_flags = 0;
*/
/*
  track->t_flags = PU_FILLER;
*/
  track->t_flags = PU_SITOUT;

  track->t_seconds = 0;
  track->t_opponent   = -1;
  track->t_wait   = 0;

  /* resets tournament stats of player */
  track->t_pos = -1;
  memset(&(track->t_stats), 0, sizeof(DogStat));
  track->t_stats.d_ticks = 1;
}


/* 
  reset this guy and his current dogfight

  This keeps the player in the game, but immediately resets
  the current dogfight he was in.  Scoring should not be
  hurt in anyway (knock on wood).
*/
void reset_dogfight(int pno)
{
  Track *track;
  Track *etrack;
  int eno;


  checkBadPlayerNo(pno,"in reset_dogfight\n");

  track = &tracks[pno];

  eno = track->t_opponent;
  track->t_opponent   = -1;
  track->t_wait   = 0;

  if (inprogress == 2)
    track->t_flags   = PU_CONTEST;
  else
    track->t_flags   = 0;

  do_teleport_home(pno);


  if (inprogress && !badPlayerNo(eno))
    {
      etrack = &tracks[eno];
  
      /* make sure opponent hasn't joined another fight already */
      if (etrack->t_opponent == pno)
	{
	  etrack->t_opponent = -1;
	  etrack->t_wait   = 0;

	  if (etrack->t_flags & PU_CONTEST)
	    etrack->t_flags   = PU_CONTEST;
	  else
	    etrack->t_flags   = 0;

	  do_teleport_home(eno);
	}
    }
}

void player_bounce(void)
{
    register int i;
    register struct player *j;
    register Track *track;
    register struct torp *t;
    Arena* a;

    /* avoid dead slots, me, other robots (which aren't hostile) */
    for (i = 0, j = &players[i], track = &tracks[i];
           i < MAXPLAYER; i++, j++, track++) {
        if ( (j == me) || (j->p_status!=PALIVE) )
            continue;  /*Don't do this for dead and for puck*/


	a = &arenas[(int) track->t_arena];

	/* kill torps that might distract guys in other arenas */

	for (t = firstTorpOf(j); t <= lastTorpOf(j); t++) 
	  if (t->t_x < a->a_left ||
	      t->t_x > a->a_right ||
	      t->t_y < a->a_top ||
	      t->t_y > a->a_bottom)
	  {
	    fprintf(stderr, "here!\n");
	    t->t_status = TFREE;
	    switch (t->t_type) {
	      case TPHOTON: j->p_ntorp--; break;
	      case TPLASMA: j->p_nplasmatorp--; break;
	    }
	  }
	p_x_y_box(j, a->a_left, a->a_top, a->a_right, a->a_bottom);
    } /*end for loop*/

}


void exitRobot(void)
{
    int i;
    struct player *j;

    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
        p_x_y_unbox(j);
    }

    if (me != NULL && me->p_team != ALLTEAM) {
	if (target >= 0) {
	    messAll(255,roboname, "I'll be back.");
	}
	else {
	    messAll(255,roboname,"#############################");
	    messAll(255,roboname,"#");
            messAll(255,roboname,"#  Venus awaits...");
	    messAll(255,roboname,"#");
	}
    }
    
    freeslot(me);
    exit(0);
}


void init_mars(void)
{
  int i;
  Arena *a;
  Track *track;
  struct player *j;

#ifdef DOGDEBUG
  ERROR(1,("init_mars\n"));
#endif
  
  /* 
    want to make sure this happens REAL FAST, want to make sure no one
    suffers from stat hosing.
  */

  for (i = 0, j = &players[i];  i < MAXPLAYER; i++, j++) 
    if (j->p_pos > 0) j->p_pos = -1;		/* Don't save player stats */

  for (i = 1, a = &arenas[1]; i < 9; i++, a++)
    {
      a->a_no = i;
      a->a_free = 1;
      a->a_top = 10000 + ((i-1) % 4 * 20000);
      a->a_bottom = 20000 + a->a_top;

      if (i-1 < 4) 
	a->a_x = 20000;
      else 
	a->a_x = 80000;

      a->a_y = 20000 + ((i-1) % 4 * 20000);

      if (i-1 < 4) 
	{
	  a->a_left = LEFT1;
	  a->a_right = RIGHT1;
	}
      else 
	{
	  a->a_left = LEFT2;
	  a->a_right = RIGHT2;
	}
    }

  a = &arenas[0];

  a->a_no = 0;
  a->a_free = 1;        /* this should never be checked, home always free */
  a->a_top = VERT2;
  a->a_bottom = VERT4;
  a->a_left = RIGHT1;
  a->a_right = LEFT2;

  for (i = 0, track = &tracks[i];
       i < MAXPLAYER; i++, track++) 
    {
      free_player(i);
      track->t_arena = 0;
      track->t_no = i;
    }
  reset_all_players();      
}

void ogg_credit(Track *track, struct player *j, int i)
{
  track->t_oggs[i]++;
  track->t_stats.d_oggs++;
  track->t_stats.d_losses++;

  recompute_kills(j);
  savedogplayer(j, track);
}

void win_credit(Track *track, struct player *j, int i)
{
  track->t_wins[i]++;
  track->t_stats.d_wins++;

  recompute_kills(j);
  savedogplayer(j, track);
}

void mutual_credit(Track *track, struct player *j, int i)
{
  track->t_mutuals[i]++;
  track->t_stats.d_mutuals++;
  track->t_stats.d_losses++;

  recompute_kills(j);
  savedogplayer(j, track);
}

void loss_credit(Track *track, struct player *j, int i)
{
  track->t_stats.d_losses++;

  recompute_kills(j);
  savedogplayer(j, track);
}


void savedogplayer(struct player *victim, Track *tr)
{
    int fd;

    if (tr->t_pos < 0) return;
    if (victim->p_stats.st_lastlogin == 0) return;
    if (victim->p_flags & PFROBOT) return;

    fd = open(DogStats, O_WRONLY, 0644);
    if (fd >= 0) {
	lseek(fd, 16 + tr->t_pos * sizeof(DogStat) ,0);
	write(fd, (char *) &tr->t_stats, sizeof(DogStat));
	close(fd);
    }
}


void get_dog_stats(struct player *j, Track *track)
{
   static DogStatEntry player;
   static int position= -1;
   int plfd;
   int entries;
   struct stat buf;

/*
   printf("dogstat: %d dogstatentry: %d\n",
	  sizeof(DogStat),sizeof(DogStatEntry));
*/

   memset(&(track->t_stats), 0, sizeof(DogStat));
   memset(&player, 0, sizeof(DogStatEntry));

   track->t_stats.d_ticks = 1;
   player.dstats.d_ticks = 1;
  
   for (;;) {     /* so I can use break; */
     plfd = open(DogStats, O_RDONLY, 0644);
     if (plfd < 0) {
       /* this might be a result of the file not existing, created below */
       ERROR(1,("I cannot open the mars player file! (will try to create)\n"));
       strncpy(player.name, j->p_name, 16);
       position= -1;
       ERROR(1,("Error number: %d\n", errno));
       break;
     }
     position=0;
     while (read(plfd, (char *) &player, sizeof(DogStatEntry)) ==
	    sizeof(DogStatEntry)) {
       if (strcmp(j->p_name, player.name)==0) {
	 close(plfd);
	 break;
       }
       position++;
     }
     if (strcmp(j->p_name, player.name)==0) break;
     close(plfd);
     position= -1;
     strncpy(player.name, j->p_name,16);
     break;
   }
   
   if (position == -1) 
     {
       strncpy(player.name, j->p_name, 16);
       memset(&player.dstats, 0, sizeof(DogStat));
       player.dstats.d_ticks = 1;
       
       plfd = open(DogStats, O_RDWR|O_CREAT, 0644);
       if (plfd < 0) 
	 {
	   ERROR(1,("Failure to access dogfight file."));
	 }
       else
	 {
	   fstat(plfd, &buf);
	   stat(DogStats,&buf);
	   entries = buf.st_size / sizeof(DogStatEntry);
	   lseek(plfd, entries * sizeof(DogStatEntry), 0); 
	   
	   write(plfd, (char *) &player, sizeof(DogStatEntry));
	   close(plfd);
	   track->t_pos = entries;
	   memcpy(&(track->t_stats), &player.dstats, sizeof(DogStat));
	 }
     }
   
   recompute_kills(j);
}





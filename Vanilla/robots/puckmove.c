/* puckmove.c
*/

/*
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
#include <signal.h>
#include <math.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "puckdefs.h"
#include "roboshar.h"
#include "puckmove.h"
#include "util.h"
#include "slotmaint.h"

#ifdef PUCK_FIRST 
#include <sys/sem.h> 

extern struct sembuf sem_puck_op[1];
extern int sem_puck;
#endif /*PUCK_FIRST */



#define WEAKER_STR 2		/* integer factor to divide tractstr down */
#define WEAKER_RNG 1.25		/* factor to divide tractrng down */

#define SIZEOF(s)		(sizeof (s) / sizeof (*(s)))
#define AVOID_TIME		4
#define AVOID_CLICKS		200

#define NORMALIZE(d) 		(((d) + 256) % 256)

#define E_INTRUDER	0x01
#define E_TSHOT		0x02
#define E_PSHOT 	0x04
#define E_TRACT 	0x08
#define NOENEMY (struct Enemy *) -1

struct Enemy {
    int e_info;
    int e_dist;
    unsigned char e_course;	/* course to enemy */
    unsigned char e_edir;	/* enemy's current heading */
    unsigned char e_tcourse;	/* torpedo intercept course to enemy */
    unsigned int e_flags;
    int e_tractor;		/* who he's tractoring/pressoring */
    int e_phrange;		/* his phaser range */
    int e_trrange;		/* his tractor range */
    struct player *pstruct;
};

typedef struct Track {		/* our record of this player */
    int t_x;
    int t_y;
    int t_status;
    unsigned int t_goals;
    unsigned int t_assists;
    unsigned int t_seconds;
    unsigned int t_flags;  /* MK - new, additional flags */
} Track;

#define PU_SITOUT      0x0001

Track tracks[MAXPLAYER];

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

int scores[MAXTEAM+1];
int currp[MAXTEAM+1];
int lastp[MAXTEAM+1];
int faceoff = FACEOFF;		/* "faceoff" mode */
int period = 1;
int roboclock = 0;
int short_game = 0;   /* 0 = long game, 1 = short game */

#define MESSFUSEVAL 50*PERSEC
static int messfuse = MESSFUSEVAL;
static char tmessage[110];

char* puckie_messages[] = {
    "Look at $n work that biscuit!",
    "Can $n shoot his way out of a paper bag?",
    "$n has the puck at $w.",
    "$n takes possession for the $ts at $w.",
    "This rink isn't big enough for the two of us, $n.",
    "The $ts are considering trading away $n.",
    "$n is actually the $t mascot.",
    "Look out, $n is racing by $w.",
    "$n is looking tired.",
    "Attention $t shoppers, Blue Light special at $w.",
    "$n shows off some impressive stickwork at $w.",
    "$n slices through $w!",
    "$n takes command of the puck.",
    "$n has turned in an impressive performance tonight.",
    "You know, the $ts should have retired $n ages ago.",
    "$n weaves through traffic...",
};



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

void cleanup(void);
void do_teleport_home(void);
void do_peace(void);
void do_msg_check(void);
void place_anncer(void);
void do_offsides(void);
void do_goal(int);
void do_score(void);
void woomp(void);
void player_maint(void);
void player_bounce(void);
void exitRobot(void);
void light_planets(void);
unsigned char	getcourse();
char *robo_message();
char *puckie_message();		/* added 8/2/91 TC */
int isInPossession(struct Enemy *enemy_buf);
int isPressoringMe(struct Enemy *enemy_buf);

#ifdef HAVE_GOALIE
struct player *ori_goalie = NULL;
struct player *kli_goalie = NULL;
static int ori_old_ship_type, kli_old_ship_type;
#endif

#ifdef OFFSIDE
int team_offsides = 0;
#endif

#ifdef PUCK_FIRST
void _rmove(void); /* original rmove we're hijacking */

void rmove(void)
{
  _rmove();
  semop(sem_puck, sem_puck_op, 1);  /* we're done, everyone else's turn now */
}

void _rmove(void)
#else /*PUCK_FIRST */
void rmove(void)
#endif /*PUCK_FIRST */
{
    extern struct Enemy *get_nearest();
    struct Enemy *enemy_buf;

    static int lastx;
    static int lasty;

    static int shotage;
    static int shotby;

    static int pauseticks;

#ifdef FO_BIAS
    static int faceoffbias = 0;
#endif /*FO_BIAS*/

    /***** Start The Code Here *****/

    me->p_ghostbuster = 0;         /* keep ghostbuster away */
    anncer->p_ghostbuster = 0;     /* keep ghostbuster away */

    /* If the game is paused, do very little */
    if (status->gameup & GU_PAUSED) {
	pauseticks++;
	if ((pauseticks % (PERSEC)) == 0) do_msg_check();
	if ((pauseticks % (10*PERSEC)) == 0)
	    messAll(me->p_no,roboname,"Game is Paused, unpause to continue");
	return;
    }

    /* Make sure game/period has not ended */
    /* This is done first to allow for a good short_game  */
    if (roboclock >= END_OF_PERIOD) {
        if (period >= PERIODS_PER_GAME) {
#ifdef FO_BIAS
            faceoffbias = 0;
#endif /*FO_BIAS*/
	    messAll(anncer->p_no,roboname,"#############################");
	    messAll(anncer->p_no,roboname,"#");
	    messAll(me->p_no,    roboname,"#  END OF GAME.");
	    messAll(anncer->p_no,roboname,"#");
	    woomp();
	} else {
#ifdef FO_BIAS
            faceoffbias = 0;
#endif /*FO_BIAS*/
	    messAll(anncer->p_no,roboname,"#");
	    messAll(me->p_no,    roboname,"#  END OF PERIOD.");
	    messAll(anncer->p_no,roboname,"#");
	    woomp();
	}
	do_score();
	roboclock = 0;
#ifdef OFFSIDE
        team_offsides = 0;
        planet_offsides(0);
#endif
	me->p_ship.s_type = STARBASE;
	me->p_whydead=KPLASMA;
	me->p_explode=10;
	me->p_status=PEXPLODE;
	me->p_whodead=0;
	if (period >= PERIODS_PER_GAME) {
	    if (scores[KLI] > scores[ORI]) 
	      messAll(me->p_no,   roboname,"# Klingons win!");
	    else if (scores[ORI] > scores[KLI])
	      messAll(me->p_no,   roboname,"# Orions win!");
	    else messAll(me->p_no,roboname,"# Tie game!");
	    messAll(anncer->p_no, roboname,"#############################");
	    scores[ORI] = 0;
	    scores[KLI] = 0;
	    period = 0;
	    length--;
	    if (length == 0)
		cleanup();
	    do_score();
	}
	period++;
    }

    /* Check that I'm alive */
    /* Messages are not checked when puck is not alive! */

    if (me->p_status != PALIVE){  /*So I'm not alive now...*/

       if (short_game) roboclock++;

       if (me->p_status == PEXPLODE) {  /*Let Puck Go BOOM */
           player_bounce();
           player_maint();
	   return;   /*Do nothing else until done exploding*/
       }
       if (me->p_whydead == KPLASMA) {  /*assume due to goal scored*/ 
	   me->p_status = PALIVE;
	   me->p_ship.s_type = SCOUT;
	   do_faceoff();
#ifdef OFFSIDE
           planet_offsides(0);          /*RESET Planets, just in case*/
#endif
           player_bounce();
           player_maint();
	   return;
       }
       cleanup();    /*Puck is dead for some unpredicted reason like xsg */
    }


    /* first priority:  is this faceoff mode?     */
    /* Messages are checked once per second here  */
    if (faceoff > 0) {
        int ticks_left;

        if (short_game){
            roboclock++;
            ticks_left = END_OF_PERIOD - roboclock - faceoff;
        }
        else
            ticks_left = END_OF_PERIOD - roboclock;

        if (ticks_left <= 10*PERSEC) {
            roboclock = END_OF_PERIOD;
            messAll(me->p_no,roboname,"  Less than 10 seconds to score, Period Ends!");
        }

        if ((faceoff % (PERSEC)) == 0) do_msg_check();
	faceoff--;
	if (faceoff == 0) {
	    int i, x, y;
	    do_war();		/* open hostilities */
	    do_offsides();
	    for (i = 0; i <= MAXTEAM; i++) {
		currp[i] = -1; lastp[i] = -1;
	    }
	    messAll(me->p_no,roboname,"<thunk!>");
	    x = 45000 + random()%10000; /* reappear */
#ifdef FO_BIAS
	    if (scores[KLI] >= (scores[ORI]+5)) faceoffbias = 1;
	    if (scores[ORI] >= (scores[KLI]+5)) faceoffbias = -1;
	    y = R_MID + FACEOFF_HELP * faceoffbias;
#else /*FO_BIAS */
	    y = R_MID;
#endif /*FO_BIAS */
	    me->p_speed = 0;	/* *** BAV *** */
            me->p_desspeed = 0;
            me->p_subspeed = 0;
	    lasty = R_MID;	/* avoid boing problems */
            lastx = x;
	    p_x_y_set(me, x, y);
	    shotby = -1;	/* to find winner of faceoff */
	}
	else if ((faceoff % (10*PERSEC)) == 0) {
	    /* maintain peace */
	    do_peace();
	    messAll(me->p_no,roboname,"Faceoff in %d seconds.", faceoff/PERSEC);
	}

	if (faceoff == FACEOFF-TELEPORT_DELAY*PERSEC) { /* move people home */
	    do_teleport_home();
	}

        player_bounce();  /* keep players where they belong  */
        player_maint();   /* update necessary stats and info */
	return;           /* Do nothing else during faceoff mode */
    }

    /* Announce Period Clock Countdown */
    /* Some useful bookkeeping stuff here */
    if ((roboclock % (PERSEC*2))   == 0) {
      place_anncer(); /*Place the announcer */
      do_msg_check(); /*Check msgs every 2 sec*/
    }
    if ((roboclock % (PERSEC*10))  == 0) do_war();
    if ((roboclock % (PERSEC*120)) == 0) {
	puck_rules();
	do_score();
    }

    if (roboclock >= (END_OF_PERIOD - 30*PERSEC)){
      int timetogo = END_OF_PERIOD - roboclock;

      if ( (timetogo % (PERSEC*10)) == 0)
	messAll(me->p_no,roboname,"  %2i seconds left in the %s",
                (timetogo/PERSEC), period>=PERIODS_PER_GAME ? "game":"period");

      if ( (timetogo < 10*PERSEC) && ((timetogo % PERSEC) == 0) )
	messAll(anncer->p_no,roboname,"  %i second%s left in the %s",
		(timetogo/PERSEC),timetogo==PERSEC ? "":"s",
		period>=PERIODS_PER_GAME ? "game":"period");
    }


    /* So we get to here:  Then puck is in play, alive, and not faceoff */

    roboclock++;       	   /* time updated PERSEC times per second */

    /* second priority:   is anyone offsides */
#ifdef OFFSIDE
    if ((lasty >= KLI_B) && (me->p_y < KLI_B)){
        messAll(me->p_no,roboname,"  The puck crosses the Kli blue line");
        check_offsides(ORI,1);
    }
    else if ((lasty <= ORI_B) && (me->p_y > ORI_B)){
        messAll(me->p_no,roboname,"  The puck crosses the Ori blue line");
        check_offsides(KLI,1);
    }
    else if ((me->p_y >= KLI_B) && (lasty < KLI_B)){
        messAll(me->p_no,roboname,"  The puck is in the neutral zone");
        planet_offsides(0);  team_offsides = 0;
    }
    else if ((me->p_y <= ORI_B) && (lasty > ORI_B)){
        messAll(me->p_no,roboname,"  The puck is in the neutral zone");
        planet_offsides(0);  team_offsides = 0;
    }
    else if (team_offsides!=0){
        check_offsides(team_offsides,2);
    }
    /* At this point, team_offsides is set to whichever team is offsides */
#endif

    /* third priority:    did we enter a goal? */

    if ((me->p_y < KLI_G) && (me->p_x > G_LFT) && (me->p_x < G_RGT)
        && (lasty >= KLI_G)) {

#ifdef OFFSIDE
        if (team_offsides==ORI){
           messAll(me->p_no,roboname,"The ORIONS have committed an offsides violation");
           penalty_offsides(team_offsides);
	}
        else{
#endif
  	   do_goal(ORI);
#ifdef FO_BIAS
	   faceoffbias = -1;
#endif /*FO_BIAS*/
#ifdef OFFSIDE
	}
#endif
        player_bounce();
        player_maint();
        return;
    }

    if ((me->p_y > ORI_G) && (me->p_x > G_LFT) && (me->p_x < G_RGT)
	&& (lasty <= ORI_G)) {

#ifdef OFFSIDE
        if (team_offsides==KLI){
           messAll(me->p_no,roboname,"The KLINGONS have committed an offsides violation");
           penalty_offsides(team_offsides);
	}
        else{
#endif
           do_goal(KLI);
#ifdef FO_BIAS
	   faceoffbias = 1;
#endif /*FO_BIAS*/

#ifdef OFFSIDE
	}
#endif
        player_bounce();
        player_maint();
	return;
    }

    /* do left/right bounce */

    if (me->p_x < RINK_LEFT) {
	me->p_x = RINK_LEFT + (RINK_LEFT - me->p_x);
	p_x_y_to_internal(me);
	me->p_dir = me->p_desdir = 64 - (me->p_dir - 192);
	shotby = -2;
    } else if (me->p_x > RINK_RIGHT) {
	me->p_x = RINK_RIGHT - (me->p_x - RINK_RIGHT);
	p_x_y_to_internal(me);
	me->p_dir = me->p_desdir = 192 - (me->p_dir - 64);
	shotby = -2;
    }

    /* do goal bounce */

    if (((me->p_y > ORI_G) && (me->p_y < ORI_E)) ||
	((me->p_y < KLI_G) && (me->p_y > KLI_E))) {
	if ((me->p_x <= G_RGT) && (lastx >= G_RGT)) {
	    if (me->p_speed > 0)
		messAll(anncer->p_no,roboname,"Boing!  Off right side of goal.");
	    me->p_x = G_RGT + (G_RGT - me->p_x);
	    p_x_y_to_internal(me);
	    me->p_dir = me->p_desdir = 64 - (me->p_dir - 192);
	    shotby = -2;
	} else if ((me->p_x >= G_LFT) && (lastx <= G_LFT)) {
	    if (me->p_speed > 0)
		messAll(anncer->p_no,roboname,"Boing!  Off left side of goal.");
	    me->p_x = G_LFT - (me->p_x - G_LFT);
	    p_x_y_to_internal(me);
	    me->p_dir = me->p_desdir = 192 - (me->p_dir - 64);
	    shotby = -2;
	}
    }
    if ((me->p_x > G_LFT) && (me->p_x < G_RGT)) {
	if ((me->p_y > KLI_E) && (lasty <= KLI_E)) {
	    if (me->p_speed > 0)
		messAll(anncer->p_no,roboname,"Boing!  Off back of Kli goal.");
	    me->p_dir = me->p_desdir = 0 - (me->p_dir - 128);
	    me->p_y = KLI_E - (me->p_y - KLI_E); 
	    p_x_y_to_internal(me);
	    shotby = -2;
	} else if ((me->p_y < ORI_E) && (lasty >= ORI_E)) {
	    if (me->p_speed > 0)
		messAll(anncer->p_no,roboname,"Boing!  Off back of Ori goal.");
	    me->p_dir = me->p_desdir = 128 - me->p_dir; 
	    me->p_y = ORI_E + (ORI_E - me->p_y);
	    p_x_y_to_internal(me);
	    shotby = -2;
	}
    }


    /* keep all players in the rink */
    /* and do some other bouncing   */
    player_bounce();
    player_maint();

    lastx = me->p_x;
    lasty = me->p_y;

    
    /* inertia:  slow down */

    if (me->p_desspeed > 0) {
	me->p_desspeed = ((float)SHOTSPEED) * (1.0/((float)(roboclock-shotage)*2/PERSEC/3.5+1.0));
	me->p_speed = me->p_desspeed;
    }
    
    /* shots: track possession, check for close pressorers */
    
    enemy_buf = get_nearest();
    if ((enemy_buf == NULL) || (enemy_buf == NOENEMY)) return;

    if (isInPossession(enemy_buf)) {

#ifdef OFFSIDE
        if (enemy_buf->pstruct->p_team == team_offsides){
	   messAll(me->p_no,roboname,"%s (%2s) committed an offsides violation.",
              enemy_buf->pstruct->p_name,enemy_buf->pstruct->p_mapchars);
           penalty_offsides(team_offsides);
	}
#endif

	/* random commentary */

	if ((random() % messfuse) == 0) {
	    messAll(anncer->p_no,roboname,puckie_message(enemy_buf->pstruct));
	    messfuse = MESSFUSEVAL;
	} else
	    if (--messfuse == 0)
		messfuse = 1;

	/* intercepts/deflects -- stop moving puck/change dir randomly */

	if (me->p_desspeed < SHOTSPEED/SHOOTABLE_FACTOR)
	    me->p_desspeed = 0;
	else {			/* deflections */
	  if ((enemy_buf->e_dist < DEFLECT_DIST) &&
	      (angdist(enemy_buf->e_course, me->p_dir) < 64) &&
	      (shotby != enemy_buf->e_info) ) {
	    /* If close and within 90 (?) degrees of course, and the
	       deflector is not the last guy to shoot it. */
	    messAll(me->p_no,roboname,"    Deflected by %s (%2s)!",
		    enemy_buf->pstruct->p_name,
		    enemy_buf->pstruct->p_mapchars);
	    me->p_desdir = me->p_desdir + random() % 192 - 96;
	    me->p_dir = me->p_desdir;
	    shotby = enemy_buf->e_info;
	  }
	}

	if (currp[enemy_buf->pstruct->p_team] != enemy_buf->e_info) {
	  /* new guy in possession */
	  lastp[enemy_buf->pstruct->p_team] = currp[enemy_buf->pstruct->p_team];
	  currp[enemy_buf->pstruct->p_team] = enemy_buf->e_info;
#ifdef ANNOUNCER
	  messAll(me->p_no,roboname,"    %s (%2s) takes possession of the puck.",
		  enemy_buf->pstruct->p_name,
		  enemy_buf->pstruct->p_mapchars);
#endif
	  
	}
    }

    if ((enemy_buf->e_dist < SHOTRANGE) &&
	(me->p_desspeed < SHOTSPEED/SHOOTABLE_FACTOR)) { /* shootable */
	
	if (isPressoringMe(enemy_buf)) {
	    /* I am being shot */
	    int shot_faceoff = (shotby == -1);
	    int shot_ongoal  = 0;

	    enemy_buf->pstruct->p_flags &= ~PFTRACT;
	    enemy_buf->pstruct->p_flags &= ~PFPRESS;
	    shotage = roboclock;
	    shotby = enemy_buf->e_info;
	    me->p_desspeed = SHOTSPEED;
	    me->p_speed = SHOTSPEED;
#ifdef SOCCER_SHOOT
	    me->p_dir = enemy_buf->e_course + 128;
	    me->p_desdir = enemy_buf->e_course + 128 ;
#else
	    me->p_dir = enemy_buf->pstruct->p_dir;
	    me->p_desdir = enemy_buf->pstruct->p_dir;
#endif

	    /* find extra information to descirbe the shot */
	    /* i.e.:  is it on goal,etc */

	    if ((enemy_buf->pstruct->p_team == KLI) &&
		(enemy_buf->pstruct->p_y >= ORI_B) &&
		(enemy_buf->pstruct->p_y <= ORI_G) &&
		(me->p_dir<=getcourse2(me->p_x,me->p_y,G_LFT,ORI_G)) &&
		(me->p_dir>=getcourse2(me->p_x,me->p_y,G_RGT,ORI_G)))
	      shot_ongoal = 1;

	    if ((enemy_buf->pstruct->p_team == ORI) &&
		(enemy_buf->pstruct->p_y >= KLI_G) &&
		(enemy_buf->pstruct->p_y <= KLI_B)){
		u_char to_left, to_right;

		to_left  = getcourse2(me->p_x,me->p_y,G_LFT,KLI_G);
		to_right = getcourse2(me->p_x,me->p_y,G_RGT,KLI_G);

		if (((me->p_dir>=to_left) && (me->p_dir<=to_right)) ||
		    ((to_right < to_left) && 
		     ((me->p_dir>=to_left) || (me->p_dir<=to_right))))
		  shot_ongoal = 1;
	      }

#ifndef SHOT_DESC
	    /* Turn off extra shot descriptions */
	    shot_ongoal = 0;
#endif

	    /* Now announce the type of shot */
	    if (shot_faceoff)
	      messAll(me->p_no,roboname,"    %s (%2s) wins the faceoff.",
		      enemy_buf->pstruct->p_name,
		      enemy_buf->pstruct->p_mapchars);
	    else if (shot_ongoal)
	      messAll(me->p_no,roboname,"    %s (%2s) shoots on goal!",
		      enemy_buf->pstruct->p_name,
		      enemy_buf->pstruct->p_mapchars);
	    else
	      messAll(me->p_no,roboname,"    %s (%2s) shoots!",
		      enemy_buf->pstruct->p_name,
		      enemy_buf->pstruct->p_mapchars);
	} else {		/* no shot yet */
	    if (enemy_buf->e_trrange > enemy_buf->e_dist)
		me->p_flags |= PFSHIELD; /* shield up */
	    else
		me->p_flags &= ~PFSHIELD;
	}			/* end shot */
    } else {		/* not shootable */
	me->p_flags &= ~PFSHIELD; /* shield down */
    }			/* end shootable */

    update_sys_defaults();

}  /* End of rmove, the main puck moving program */

/*  
 *  Additional routine start here
 */

void cleanup(void)
{
    register struct player *j;
    register int i;

    /* Free the held slots - MK 9-20-92 */
    queues[QU_PICKUP].free_slots = queues[0].free_slots
                            + (MAXPLAYER - TESTERS) - NUMSKATERS;
    queues[QU_PICKUP].max_slots = MAXPLAYER - TESTERS;
    queues[QU_PICKUP].tournmask = ALLTEAM;
    queues[QU_PICKUP].high_slot = 0;
    queues[QU_PICKUP].high_slot = MAXPLAYER - TESTERS;

    if (!practice) {
	memcpy(planets, oldplanets, sizeof(struct planet) * MAXPLANETS);
	for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	    if ((j->p_status != PALIVE) || (j == me)) continue;
	    getship(&(j->p_ship), j->p_ship.s_type);
	}
    }
    status->gameup &= ~GU_PUCK;
    exitRobot();
}

void do_teleport_home(void)
{
    register int i;
    register struct player *j;
    int startplanet;

    if (practice) return;
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {

        /*Cleaned up this routine for v2 - MK*/

   	tracks[i].t_x = 0;  /* **BAV** Don't rewarp if player reenters */

	if ((j->p_status != PALIVE) || (j == me)) continue;

	if (j->p_team == KLI)
	    startplanet = 20;
	else if (j->p_team == ORI)
	    startplanet = 30;
	else            
	    continue;

	j->p_y = planets[startplanet].pl_y + (random() % (2*DISPLACE) - DISPLACE);
	if (j->p_x >= RINK_LEFT && j->p_x <= RINK_RIGHT)
	    j->p_x = planets[startplanet].pl_x + (random() % (2*DISPLACE) - DISPLACE);
	p_x_y_to_internal(j);

	j->p_speed = 0;
	j->p_desspeed = 0;
	j->p_subspeed = 0;
	j->p_flags = PFSHIELD;
	j->p_fuel   = j->p_ship.s_maxfuel;
	j->p_shield = j->p_ship.s_maxshield;
	j->p_damage = 0;

        tracks[i].t_x = j->p_x;
        tracks[i].t_y = j->p_y;

    }
}

void do_peace(void)
{
    register int i;
    register struct player *j;

    if (practice) return;
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	j->p_hostile = 0;
	j->p_swar = 0;
	j->p_war = 0;
	j->p_ship.s_tractstr = 1;
    }
}

void do_msg_check(void)
{
    while (oldmctl!=mctl->mc_current) {
	oldmctl++;
	if (oldmctl==MAXMESSAGE) oldmctl=0;
	if (messages[oldmctl].m_flags & MINDIV) {
	    if ( (messages[oldmctl].m_recpt == me->p_no) ||
                 (messages[oldmctl].m_recpt == anncer->p_no) ){
		check_command(&messages[oldmctl]);

#ifdef HAVE_GOALIE
		if (strstr(messages[oldmctl].m_data, "UNGOALIE") != NULL)
		{
		    do_ungoalie(messages[oldmctl].m_from); /* ***RAY*** */
		    return; /* else we also will do the GOALIE test */
		}
		/* if (strstr(messages[oldmctl].m_data, "GOALIE") != NULL) */
		if (strstr(messages[oldmctl].m_data, "GOALIE") != NULL)
		    do_goalie(messages[oldmctl].m_from); /* ***RAY*** */
#endif

	    }
	} else if (messages[oldmctl].m_flags & MALL) {
	    if (strstr(messages[oldmctl].m_data, "help") != NULL)
		messOne(me->p_no,roboname,messages[oldmctl].m_from,
                   "If you want help from me, send me the message 'help'.");
	}
    }
}


#ifdef HAVE_GOALIE
/* goalie/ungoalie functions ***RAY*** */

void do_goalie(int who)
{
    if (players[who].p_team == ORI)
    {
	if (ori_goalie != NULL)
	{
	    messOne(me->p_no,roboname,who, "Your team already has a goalie: %2s.", ori_goalie->p_mapchars);
	    return;
	}
	if (players[who].p_y < ORI_B)
	{
	    messOne(me->p_no,roboname,who, "You have to be behind your home planet to be the goalie.");
	    return;
	}
	ori_goalie = &(players[who]);
	ori_old_ship_type = ori_goalie->p_ship.s_type;
	getship(&(ori_goalie->p_ship), BATTLESHIP);
	ori_goalie->p_ship.s_repair = 30000;
	ori_goalie->p_ship.s_type = ASSAULT;

        ori_goalie->p_ship.s_maxdamage = 999;
        ori_goalie->p_ship.s_maxshield = 999;
        /*ori_goalie->p_ship.s_maxspeed = 6;*/     /* Was 8 -JGR */
        /*ori_goalie->p_ship.s_maxegntemp = 600;*/ /* Was 1000 -JGR */
        /*ori_goalie->p_ship.s_egncoolrate = 3;*/  /* Was 6 -JGR */

	messAll(me->p_no,roboname,"%s (%2s) comes in as goalie for the Orions.", players[who].p_name, players[who].p_mapchars);
    }
    else
    {
	if (kli_goalie != NULL)
	{
	    messOne(me->p_no,roboname,who, "Your team already has a goalie: K%c.", shipnos[kli_goalie->p_no]);
	}
	if (players[who].p_y > KLI_B)
	{
	    messOne(me->p_no,roboname,who,"You have to be behind your home planet to be the goalie.");
	    return;
	}
	kli_goalie = &(players[who]);
	kli_old_ship_type = kli_goalie->p_ship.s_type;
	getship(&(kli_goalie->p_ship), BATTLESHIP);
	kli_goalie->p_ship.s_repair = 30000;
	kli_goalie->p_ship.s_type = ASSAULT;

	kli_goalie->p_ship.s_maxdamage = 999;
        kli_goalie->p_ship.s_maxshield = 999;
        /*kli_goalie->p_ship.s_maxspeed = 6;*/   /* Was 8 -JGR */
        /*kli_goalie->p_ship.s_maxegntemp = 600;*/ /* Was 1000 -JGR */
        /*kli_goalie->p_ship.s_egncoolrate = 3;*/ /* Was 6 -JGR */

	messAll(me->p_no,roboname,"%s (K%c) comes in as goalie for the Klingons.", players[who].p_name, shipnos[who]);
    }
}

void do_ungoalie(int who)
{
    if (players[who].p_team == ORI)
    {
	if ((ori_goalie == NULL) || (ori_goalie->p_no != who))
	{
	    messOne(me->p_no,roboname,who,"You aren't the goalie!");
	    return;
	}
	messAll(me->p_no,roboname,"%s (%2s) is no longer guarding the net.", ori_goalie->p_name, players[who].p_mapchars);
	getship(&(ori_goalie->p_ship), ori_old_ship_type);
        ori_goalie->p_shield = ori_goalie->p_ship.s_maxshield / 2;
        ori_goalie->p_damage = ori_goalie->p_ship.s_maxdamage / 2;
        ori_goalie->p_fuel = ori_goalie->p_ship.s_maxfuel / 2;
	ori_goalie = NULL;
    }
    else
    {
	if ((kli_goalie == NULL) || (kli_goalie->p_no != who))
	{
	    messOne(me->p_no,roboname,who,"You aren't the goalie!");
	    return;
	}
	messAll(me->p_no,roboname,"%s (K%c) is no longer guarding the net.", kli_goalie->p_name, shipnos[who]);
	getship(&(kli_goalie->p_ship), kli_old_ship_type);
        kli_goalie->p_shield = kli_goalie->p_ship.s_maxshield / 2;
        kli_goalie->p_damage = kli_goalie->p_ship.s_maxdamage / 2;
        kli_goalie->p_fuel = kli_goalie->p_ship.s_maxfuel / 2;
	kli_goalie = NULL;
    }
}

#endif

/* Added by MK */
/* ARGSUSED */
void do_version(char *comm, struct message *mess)
{
    int who;

    who = mess->m_from;

    messOne(me->p_no,roboname,who, "Puck Version 2.6Bpl2 - Beta Release Version");
    messOne(me->p_no,roboname,who, "Please mail bug/coments to kantner@hot.caltech.edu");
  }


/* ARGSUSED */
void do_stats_msg(char *comm, struct message *mess)
{
    int who;
    register int i;
    register Track *track;
    register struct player *j;

    who = mess->m_from;

    for (i = 0, j = &players[i], track = &tracks[i];
	 i < MAXPLAYER; i++, j++, track++) {
	if ((j->p_status != PFREE) && !(j->p_flags & PFROBOT)) {
	    messOne(me->p_no,roboname,who, " %2s: %-16s  %2dG %2dA %3d min",
		    j->p_mapchars,
		    j->p_name,
		    track->t_goals,
		    track->t_assists,
		    track->t_seconds / 60);
	}
    }
}

/* ARGSUSED */
void do_change_timekeep(int who, int mflags, int sendto)
{
  if (short_game){
    short_game = 0;
    messAll(me->p_no,roboname,"Changing Time Keeping Method to LONG game");
  }
  else{
    short_game = 1;
    messAll(me->p_no,roboname,"Changing Time Keeping Method to SHORT game");
  }
}

/* ARGSUSED */
void do_force_newgame(int who, int mflags, int sendto)
{
    register int i;
    register Track *track;
    register struct player *j;

	roboclock = END_OF_PERIOD;
	period = PERIODS_PER_GAME;
/****MK - stats are cleared*/
#ifdef NEWGAME_RESET
        messAll(me->p_no,roboname,"Clearing Player Stats for the Newgame");
        for (i = 0, j = &players[i], track = &tracks[i];
	       i < MAXPLAYER; i++, j++, track++) {
	   if ( !(j->p_flags & PFROBOT) ) {
   		    track->t_goals   = 0;
		    track->t_assists = 0;
		    track->t_seconds = 0;
	   }
        }
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
	if ((j->p_status == PALIVE) && (j != me) && (j != anncer) ) {
	    tracks[i].t_seconds += 10;
	    j->p_hostile |= (KLI|ORI);
	    j->p_hostile &= ~j->p_team;
	    j->p_war = (j->p_swar | j->p_hostile);
	    j->p_armies = 0;
	    j->p_ship.s_egncoolrate = 12;
/*	    j->p_ship.s_tractstr = 2500; non-uniform tractor str? */
/*	    j->p_ship.s_tractrng = 0.9;  */
	    switch (j->p_ship.s_type) {
	      case SCOUT:
		j->p_ship.s_tractstr = 2000/WEAKER_STR; /* scout:  */
                j->p_ship.s_tractrng = 0.7/WEAKER_RNG;
		break;
	      case DESTROYER:
		j->p_ship.s_tractstr = 2500/WEAKER_STR; /* destroyer: */
                j->p_ship.s_tractrng = 0.9/WEAKER_RNG;
		break;
	      case BATTLESHIP:
	      case ASSAULT:
		j->p_ship.s_tractstr = 3700/WEAKER_STR; /* btlship/assault: */
                j->p_ship.s_tractrng = 1.2/WEAKER_RNG;
		break;
	      case STARBASE:
		j->p_ship.s_tractstr = 8000/WEAKER_STR; /* starbase */
                j->p_ship.s_tractrng = 1.5/WEAKER_RNG;
		break;
	      case ATT:
		j->p_ship.s_tractstr = 32765/WEAKER_STR; /* att: */
		break;
	      default:
		j->p_ship.s_tractstr = 3000/WEAKER_STR; /* cruiser: */
                j->p_ship.s_tractrng = 1.0/WEAKER_RNG;
		break;

	    }
	}
    }
}

void place_anncer(void)
{
      anncer->p_status = PALIVE;
      anncer->p_speed = 0;        /* keep the announcer in place */
      anncer->p_desspeed = 0;
      anncer->p_x = RINK_LEFT/4;
      anncer->p_y = R_MID;
      p_x_y_to_internal(anncer);
}

void place_sitout(int who)
{
    struct player *j;
    Track *track;

    j = &players[who];
    track = &tracks[who];

    /*Place people on opposite sides of the rink*/
    if (j->p_team == KLI){
       j->p_x = track->t_x = SITOUT_X;
#ifdef SITOUT_HURTS
       j->p_y = track->t_y = KLI_B + (random() % (2*DISPLACE) - DISPLACE);
#endif /*SITOUT_HURTS */
       p_x_y_to_internal(j);
    }
    else if (j->p_team == ORI){
       j->p_x = track->t_x = GWIDTH - SITOUT_X;
#ifdef SITOUT_HURTS
       j->p_y = track->t_y = ORI_B + (random() % (2*DISPLACE) - DISPLACE);
#endif /*SITOUT_HURTS*/
       p_x_y_to_internal(j);
    }
    else 
       return;  /*Not ORI, not KLI, so do nothing... Weird*/

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
    if (track->t_flags & PU_SITOUT){
       messOne(me->p_no,roboname,who,"You are already sitting out");
       return;
     }

    place_sitout(who);

    messAll(me->p_no,roboname,"%s (%2s) is sitting out.", players[who].p_name, players[who].p_mapchars);
 
    track->t_flags |= PU_SITOUT ;

    j->p_speed = 0;                      /*So you don't move        */
    j->p_desspeed = 0;
    j->p_subspeed = 0;
    j->p_flags = PFSHIELD;               
#ifdef SITOUT_HURTS
    j->p_fuel   = 0;                     /*Hah, you're hosed anyway */
    j->p_damage = j->p_ship.s_maxdamage - 5;
    j->p_shield = j->p_ship.s_maxshield; /*OK, a potential help     */
#endif /*SITOUT_HURTS*/
  }

void do_offsides(void)
{
    register int i;
    register struct player *j;

    if (practice) return;
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	if ((j != me) && (j->p_status == PALIVE)) {
	    if (((j->p_team == KLI) && (j->p_y > GWIDTH/2)) ||
		((j->p_team == ORI) && (j->p_y < GWIDTH/2))) {
		int startplanet = 10;
		messAll(me->p_no,roboname,"%s (%2s) is offsides.", j->p_name,
			j->p_mapchars);
		j->p_flags &= ~(PFORBIT|PFPLOCK|PFPLLOCK|PFTRACT|PFPRESS);  /* ***BAV*** */
		if (j->p_team == KLI)
		    startplanet = 20;
		else if (j->p_team == ORI)
		    startplanet = 30;
		j->p_y = planets[startplanet].pl_y + (random() % (2*DISPLACE) - DISPLACE);
		p_x_y_to_internal(j);
	    }
	}
    }
}


void puck_rules(void)
{
    messAll(anncer->p_no,roboname,"This is Netrek hockey.  Send me the message \"help\" for rules.");
}

char* team_names[MAXTEAM+1] = { "", "Federation", "Romulans", "", "Klingons",
				   "", "", "", "Orions" };
void do_goal(int team)
{

#ifdef NO_BRUTALITY
    do_peace();
#endif

    messAll(anncer->p_no,roboname,"*****************************************************************");

    if (lastp[team] == -1) {	/* unassisted */
	if (currp[team] == -1) { /* untouched?! */
	    messAll(me->p_no,roboname,"* The %s score without touching the puck!?",
		    team_names[team]);
	} else {
	    messAll(me->p_no,roboname,"* %s scores for the %s!  Unassisted!",
		    players[currp[team]].p_name, team_names[team]);
	    tracks[currp[team]].t_goals++;
#ifndef LTD_STATS
	    if (status->tourn) { 
		players[currp[team]].p_stats.st_tplanets++;
		status->planets++;
	    } else {
		players[currp[team]].p_stats.st_planets++;
	    }
#endif
	}
    } else {
	messAll(me->p_no,roboname, "* %s scores for the %s!  %s assisting!",
		players[currp[team]].p_name, team_names[team],
		players[lastp[team]].p_name);
	tracks[currp[team]].t_goals++;
	tracks[lastp[team]].t_assists++;
#ifndef LTD_STATS
	if (status->tourn) {
	    players[currp[team]].p_stats.st_tplanets++;
	    players[lastp[team]].p_stats.st_tarmsbomb++;
	    status->planets++;
	    status->armsbomb++;
	} else {
	    players[currp[team]].p_stats.st_planets++;
	    players[lastp[team]].p_stats.st_armsbomb++;
	}
#endif
    }
    switch (random()%11) {
      case 0:
	messAll(anncer->p_no,roboname,"* Get in the fast lane, Gramma, the bingo game is ready to roll!"); break;
      case 1:
	messAll(anncer->p_no,roboname,"* Scratch my back with hacksaw!"); break;
      case 2:
	messAll(anncer->p_no,roboname,"* He beat him like a rented mule!"); break;
      case 3:
	messAll(anncer->p_no,roboname,"* Book'em Danno!"); break;
      case 4:
	messAll(anncer->p_no,roboname,"* She wants to sell my monkey!"); break;
      case 5:
	messAll(anncer->p_no,roboname,"* Ladies and gentlemen, Elvis has just left the building!"); break;
      case 6:
	messAll(anncer->p_no,roboname,"* Look out Loretta!"); break;
      case 7:
	messAll(anncer->p_no,roboname,"* How now Eddie Spaghetti!"); break;
      case 8:
	messAll(anncer->p_no,roboname,"* Michael Michael motorcycle!"); break;
      case 9:
	messAll(anncer->p_no,roboname,"* Stop the press, it's over now!"); break;
      case 10:  /* *** BAV *** */
	messAll(anncer->p_no,roboname,"* Looky, Looky!  Here comes Cookie!"); break;
    }
    messAll(anncer->p_no,roboname,"*****************************************************************");
    scores[team]++;
    do_score();
    me->p_ship.s_type = STARBASE;
    me->p_whydead=KPLASMA;
    me->p_explode=10;
    me->p_status=PEXPLODE;
    me->p_whodead=0;
/*    do_faceoff();*/
}

void do_score(void)
{
    int tempclock;

    tempclock = roboclock/PERSEC;
    messAll(me->p_no,roboname,"Score is Klis %d, Oris %d.  Elapsed time %02d:%02d.  Period %d.",
	    scores[KLI], scores[ORI], tempclock/60, tempclock%60, period);
#ifdef LIGHT_PLANETS
    light_planets();
#endif
}

/* ARGSUSED */
void do_score_msg(char *comm, struct message *mess)
{
    int who;
    int tempclock;

    who = mess->m_from;

    tempclock = roboclock/PERSEC;
    messOne(me->p_no,roboname,who, "Score is Klis %d, Oris %d.  Elapsed time %02d:%02d.  Period %d.",
	    scores[KLI], scores[ORI], tempclock/60, tempclock%60, period);
}

void do_faceoff(void)
{
    register int i,l;
    register struct player *j;
    
    for (i = 0, j = &players[i]; 
	i < MAXPLAYER; i++, j++) {
	if (j == me)  continue;
	for (l=0; l < PV_TOTAL; l++) j->voting[l] = -1;
    }
    me->p_speed = 0;
    me->p_desspeed = 0;
    me->p_x = RINK_LEFT;	/* *** BAV *** */
    me->p_y = R_MID;
    p_x_y_to_internal(me);
    place_anncer();
    if (debug)
	faceoff = 5;
    else
	faceoff = FACEOFF;
}
	
unsigned char getcourse(int x, int y)
{
	return((unsigned char) rint((atan2((double) (x - me->p_x),
	    (double) (me->p_y - y)) / 3.14159 * 128.)));
}

int isInPossession(struct Enemy *enemy_buf)
{
    return ((enemy_buf->pstruct->p_flags & PFTRACT) &&
	    (enemy_buf->pstruct->p_tractor == me->p_no));
}

int isTractoringMe(struct Enemy *enemy_buf)
{
    return ((enemy_buf->pstruct->p_flags & PFTRACT) &&
	    !(enemy_buf->pstruct->p_flags & PFPRESS) &&
	    (enemy_buf->pstruct->p_tractor == me->p_no));
}

int isPressoringMe(struct Enemy *enemy_buf)
{
    return ((enemy_buf->pstruct->p_flags & PFTRACT) &&
	    (enemy_buf->pstruct->p_flags & PFPRESS) &&
	    (enemy_buf->pstruct->p_tractor == me->p_no));
}

void woomp(void)
{
    register int i;
    register struct player *j;

    /* avoid dead slots, me, other robots (which aren't hostile) */
    for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	if (j == me || j->p_status==PFREE)  /* Last clause *** BAV *** */
	    continue;
	j->p_ship.s_type = STARBASE;
	j->p_whydead=KPLASMA;
	j->p_explode=10;
	j->p_status=PEXPLODE;
	j->p_whodead=me->p_no;
    }
}

/* by MK */
void player_maint(void)
{
    register int i,l;
    register struct player *j;
    register Track *track;

    /* avoid dead slots, me, other robots (which aren't hostile) */
    for (i = 0, j = &players[i], track = &tracks[i];
           i < MAXPLAYER; i++, j++, track++) {

        /* */
        /*RESURRECTION code is in here!!!*/
        /* */

	if ((j == me) || (j == anncer))/* ignore puck */
	    continue;

	if (j->p_status != PALIVE) {       /*for dead people*/
	    track->t_status = j->p_status;
	    if (j->p_status == PFREE) {
		track->t_goals = 0;
		track->t_assists = 0;
		track->t_seconds = 0;
		track->t_x = 0;
		track->t_y = 0;
                for (l = 0; l < PV_TOTAL; l++) j->voting[l] = -1;
                track->t_flags = 0;
	    }
	    continue;		/* do nothing else for dead players */
	}

        /*At this point, the player is alive*/	

        /*Place players who just came back to life*/
	if ((track->t_status != PALIVE) && (track->t_x != 0)) {
	    if (!practice) {
		if (random() % 2 == 0) /* step to the left */
		    j->p_x = track->t_x - ((random() % DXDISPLACE) + DXOFFSET);
		else /* step to the right */
		    j->p_x = track->t_x + ((random() % DXDISPLACE) + DXOFFSET);
		/* and shake it all around */
		j->p_y = track->t_y + random()%(2*DYDISPLACE) - DYDISPLACE;
		p_x_y_to_internal(j);
	    }
	}

        /*If you were sitting out and died, you are still sitting out*/	
	if ((track->t_status != PALIVE) && (track->t_flags & PU_SITOUT)) {
	    if (!practice) {
                place_sitout(i);
	    }
	}

        if (track->t_flags & PU_SITOUT){
           if ( (j->p_x >= RINK_LEFT) && (j->p_x <= RINK_RIGHT) ){
              track->t_flags &= ~PU_SITOUT;
              messAll(me->p_no,roboname,"%s (%2s) is back on the ice.",
                       players[i].p_name, players[i].p_mapchars);
	    }
	 }

        /* Save current info */
	track->t_status = j->p_status;
	track->t_x = j->p_x;
	track->t_y = j->p_y;

        /* Note: track->t_seconds is updated in do_war */

    }
}

void player_bounce(void)
{
    register int i;
    register struct player *j;
    register Track *track;

    /* avoid dead slots, me, other robots (which aren't hostile) */
    for (i = 0, j = &players[i], track = &tracks[i];
           i < MAXPLAYER; i++, j++, track++) {
        if ( (j == me) || (j == anncer) || (j->p_status!=PALIVE) ||
             (j->p_flags & PFORBIT) )
            continue;  /*Don't do this for dead and for puck*/
        if (track->t_flags & PU_SITOUT)
	    continue;  /*Don't do this if sitting out */

#ifdef WALL_BOUNCE
        /* Hit/bounce off the boards */
        if (j->p_x < RINK_LEFT) {
           j->p_x = RINK_LEFT;
           p_x_y_to_internal(j);
        } else if (j->p_x > RINK_RIGHT) {
           j->p_x = RINK_RIGHT;
           p_x_y_to_internal(j);
        }
#endif

#ifdef GOAL_CLEAR
        /* do goal bounce? */ 
        /* is player "inside a goal?" */
        if ( ((j->p_x > G_LFT) && (j->p_x < G_RGT)) &&
                   ( ((j->p_y > ORI_G) && (j->p_y < ORI_E)) ||
	             ((j->p_y < KLI_G) && (j->p_y > KLI_E))    ) ) {

    	   if (track->t_x >= G_RGT) {
               j->p_x = G_RGT;         /*Hits right side of goal*/
               p_x_y_to_internal(j);
	   } else if (track->t_x <= G_LFT) {
               j->p_x = G_LFT;         /*Hits left side of goal*/
               p_x_y_to_internal(j);
	   }

           if ((j->p_y > KLI_E) && (track->t_y <= KLI_E)) {
               j->p_y = KLI_E;         /*Hits back of Kli goal*/
               p_x_y_to_internal(j);
	   } else if ((j->p_y < ORI_E) && (track->t_y >= ORI_E)) {
               j->p_y = ORI_E;         /*Hits back of Ori goal*/
               p_x_y_to_internal(j);
	   } else if ((j->p_y < KLI_G) && (track->t_y >= KLI_G)) {
               j->p_y = KLI_G;         /*Hits front of Kli goal*/
               p_x_y_to_internal(j);
	   } else if ((j->p_y > ORI_G) && (track->t_y <= ORI_G)) {
               j->p_y = ORI_G;         /*Hits front of Ori goal*/
               p_x_y_to_internal(j);
	   }

        }  /*End big inside goal if */
#endif

    } /*end for loop*/

#ifdef HAVE_GOALIE
    /* check goalies  ***RAY*** */
    if (ori_goalie != NULL)
    {
	if (ori_goalie->p_y < ORI_B)
	    ori_goalie->p_y = ORI_B;
	if (ori_goalie->p_status != PALIVE) 
	{
	    ori_goalie->p_ship.s_type = ori_old_ship_type;
	    ori_goalie = NULL;
	}
    }
    if (kli_goalie != NULL)
    {
	if (kli_goalie->p_y > KLI_B)
	    kli_goalie->p_y = KLI_B;
	if (kli_goalie->p_status != PALIVE) 
	{
	    kli_goalie->p_ship.s_type = kli_old_ship_type;
	    kli_goalie = NULL;
	}
    }
#endif

}

#ifdef OFFSIDE
void check_offsides(int team, int offside_msg)
{
   register int i;
   register struct player *j;
   register Track *track;

   team_offsides = 0;

   for (i=0, j=&players[i]; i < MAXPLAYER; i++, j++){
      if ( (j->p_status != PALIVE) || (j==me) ) continue;

      if ( (team==ORI) && (j->p_y < KLI_B) && (j->p_team==ORI) ||
           (team==KLI) && (j->p_y > ORI_B) && (j->p_team==KLI) ){
         team_offsides = team;
         if (offside_msg==1)
            messAll(me->p_no,roboname,"%s (%2s) is offsides.", j->p_name,j->p_mapchars);
      }
   }

   if ( (offside_msg==2) && (team!=team_offsides) )
      messAll(me->p_no,roboname,"The %s are longer offsides!",team==ORI?"Orions":"Klingons");

   planet_offsides(team_offsides);

}

void penalty_offsides(int team)
{
   do_teleport_home();               /*Send everyone back to start*/
   planet_offsides(0);  team_offsides = 0;  /*Clear Offsides*/

   /*Place Puck in Neutral Zone*/
   if (me->p_x > G_MID)  me->p_x = RED_4;
   else                  me->p_x = RED_1;

   if (team == ORI)      me->p_y = (R_MID + KLI_B)/2;
   else                  me->p_y = (R_MID + ORI_B)/2;
   p_x_y_to_internal(me);
  
   me->p_speed=me->p_desspeed=0;
}

void planet_offsides(int team)
{
#define IND 0

   register int i;

   if (team == KLI){                  /*Klingons are offsides*/
      planets[37].pl_owner = ORI;     /* Urs */
      planets[38].pl_owner = ORI;     /* Her */
      for (i=7; i<10; i++){
         planets[i].pl_owner    = ORI;
         planets[i+10].pl_owner = ORI;
       }
   }else if (team == ORI){            /*Orions are offsides*/
      planets[23].pl_owner = KLI;     /* Lal */
      planets[29].pl_owner = KLI;     /* Cas */
      for (i=0; i<3; i++){
         planets[i].pl_owner    = KLI;
         planets[i+10].pl_owner = KLI;
       }
   }else {                            /*No one is offsides now*/
      planets[37].pl_owner = IND;
      planets[38].pl_owner = IND;
      planets[23].pl_owner = IND;
      planets[29].pl_owner = IND;
      for (i=0; i<10; i++){
         planets[i].pl_owner    = IND;
         planets[i+10].pl_owner = IND;
       }

   }
}
#endif

struct Enemy ebuf;

struct Enemy *get_nearest(void)
{
    int pcount = 0;
    register int i;
    register struct player *j;
    int intruder = 0;
    int tdist;
    double dx, dy;

    /* Find an enemy */
    ebuf.e_info = me->p_no;
    ebuf.e_dist = GWIDTH + 1;

    pcount = 0;  /* number of human players in game */

    /* avoid dead slots, me, other robots (which aren't hostile) */
    for (i = 0, j = &players[i];
	 i < MAXPLAYER; i++, j++) {

        /*skip me, announcer, and the not-alive players */
        if ( (j==me) || (j==anncer) || (j->p_status != PALIVE) ) continue;

	pcount++;	/* Other players in the game */

	/* We have an enemy */
	/* Get his range */
	dx = j->p_x - me->p_x;
	dy = j->p_y - me->p_y;
	tdist = hypot(dx, dy);
	
	if (tdist < ebuf.e_dist) {
	    ebuf.e_info = i;
	    ebuf.e_dist = tdist;
	    if (intruder)
		ebuf.e_flags |= E_INTRUDER;
	    else
		ebuf.e_flags &= ~(E_INTRUDER);
	}
    }			/* end for */
    if (pcount == 0) {
	return (NOENEMY);    /* no players in game */
    } else if (ebuf.e_info == me->p_no) {
	return (0);			/* no hostile players in the game */
    } else {
	j = &players[ebuf.e_info];
	ebuf.pstruct = j;

	/* Get torpedo course to nearest enemy */
	ebuf.e_flags &= ~(E_TSHOT);

	/* Get phaser shot status */
	if (ebuf.e_dist < 0.8 * phrange)
		ebuf.e_flags |= E_PSHOT;
	else
		ebuf.e_flags &= ~(E_PSHOT);

	/* Get tractor/pressor status */
	if (ebuf.e_dist < trrange)
		ebuf.e_flags |= E_TRACT;
	else
		ebuf.e_flags &= ~(E_TRACT);


	/* get his phaser and tractor range */
	ebuf.e_phrange = PHASEDIST * j->p_ship.s_phaserdamage / 100;
	ebuf.e_trrange = TRACTDIST * j->p_ship.s_tractrng;

	/* get course info */
	ebuf.e_course = getcourse(j->p_x, j->p_y);
	ebuf.e_edir = j->p_dir;
	ebuf.e_tractor = j->p_tractor;
	/*
	if (debug)
	    ERROR(1,( "Set course to enemy is %d (%d)\n",
		    (int)ebuf.e_course, 
		    (int) ebuf.e_course * 360 / 256));
	*/

	return (&ebuf);
    }
}

char* puckie_message(struct player* enemy)
{
    int i;
    char *s,*t;
    
    i=(random() % (sizeof(puckie_messages)/sizeof(puckie_messages[0])));

    for (t=puckie_messages[i], s=tmessage; *t!='\0'; s++, t++) {
	switch(*t) {
	case '$':
	    switch(*(++t)) {
		case '$':
		    *s = *t;
		    break;
		case 'T':    /* "a Fed" or "a Klingon" ... */
		    if (enemy->p_team != me->p_team &&
			enemy->p_team == ORI) {
			strcpy(s, "an ");
		    } else {
			strcpy(s, "a ");
		    }
		    s=s+strlen(s);
		case 't':    /* "Fed" or "Orion" */
                    if (enemy->p_team != me->p_team) {
	                switch (enemy->p_team) {
			  case FED:
	                    strcpy(s, "Fed");
	                    break;
			  case ROM:
	                    strcpy(s, "Romulan");
	                    break;
			  case KLI:
	                    strcpy(s, "Klingon");
	                    break;
			  case ORI:
	                    strcpy(s, "Orion");
	                    break;
	                }
                    } else {
	                strcpy(s, "TRAITOR");
                    }
		    s=s+strlen(s)-1;
		    break;
		  case 'n':	/* name 8/2/91 TC */
		    strcpy(s, enemy->p_name);
		    s=s+strlen(s)-1;
		    break;
		  case 'w':	/* where on the rink */
		    if (me->p_y < KLI_G)
			strcpy(s, "the Klingon goal end");
		    else if (me->p_y < KLI_B)
			strcpy(s, "the Klingon zone");
		    else if (me->p_y < ORI_B)
			strcpy(s, "the neutral zone");
		    else if (me->p_y < ORI_G)
			strcpy(s, "the Orion zone");
		    else
			strcpy(s, "the Orion goal end");
		    s=s+strlen(s)-1;
		    break;
		  default:
		    *s = *t;
	    }
	    break;
	default:
	    *s = *t;
	    break;
	}
    }
    *s='\0';
    return(tmessage);
    
}

void exitRobot(void)
{
    if (me != NULL && me->p_team != ALLTEAM) {
	if (target >= 0) {
	    messAll(me->p_no,roboname, "I'll be back.");
	}
	else {
	    messAll(me->p_no,roboname,"#############################");
	    messAll(me->p_no,roboname,"#");
            messAll(me->p_no,roboname,"#  The puck is tired.  Hockey is over for now");
	    messAll(me->p_no,roboname,"#");
	}
    }

#ifdef PUCK_FIRST
    if (sem_puck > -1) 
      {
	semop(sem_puck, sem_puck_op, 1);
      }
#endif /*PUCK_FIRST*/
    
    freeslot(me);
    freeslot(anncer);
    exit(0);
}

#ifdef LIGHT_PLANETS
/*
 * light_planets changes ownership of planets to indicate the score.
 * Some people really like this feature, and no one objects to it.
 */


#define IND 0
void light_planets(void)
{
    int pln_light,extra;
    int i;
    int kli_light= ROM, ori_light = FED;

    if (scores[KLI] > 0) {
       pln_light=scores[KLI]%5;
       switch(pln_light) {
          case 1: 
             planets[10].pl_owner=kli_light; 
             for (i=11; i<15; i++) {
                planets[i].pl_owner = IND;} 
             extra = scores[KLI]/5;
             if (extra > 0) { planets[extra-1].pl_owner = kli_light;}
             break;
          case 2: 
             planets[11].pl_owner=kli_light; break;
          case 3: 
             planets[12].pl_owner=kli_light; break;
          case 4: 
             planets[13].pl_owner=kli_light; break;
          case 0: 
             planets[14].pl_owner=kli_light; break;
          default:
             break;
       }
    }else{
       for (i=0; i<5; i++) {
          planets[i].pl_owner = IND;
          planets[10+i].pl_owner = IND; }
    }

    if (scores[ORI] > 0) {
       pln_light=scores[ORI]%5;
          switch(pln_light) {
             case 1: 
                planets[9].pl_owner=ori_light; 
	        for (i=5; i<9; i++) {
                   planets[i].pl_owner = IND;} 
                extra = scores[ORI]/5;
                if (extra > 0) {
                   planets[20-extra].pl_owner = ori_light;}
                break;
             case 2: 
                planets[8].pl_owner=ori_light; break;
             case 3: 
                planets[7].pl_owner=ori_light; break;
             case 4: 
                planets[6].pl_owner=ori_light; break;
             case 0: 
                planets[5].pl_owner=ori_light; break;
             default:
                break;
          }
    }else{
       for (i=0; i<5; i++) {
          planets[5+i].pl_owner = IND;
          planets[15+i].pl_owner = IND; }
    }

}

#endif /* LIGHT_PLANETS */

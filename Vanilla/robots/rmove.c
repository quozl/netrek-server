/* rmove.c
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
#include "roboshar.h"
#include "util.h"
#include "slotmaint.h"


#define SIZEOF(s)		(sizeof (s) / sizeof (*(s)))
#define AVOID_TIME		4
#define AVOID_CLICKS		200
#define BOREDOM_TIME		300
#define NORMALIZE(d) 		(((d) + 256) % 256)

#define E_INTRUDER	0x01
#define E_TSHOT		0x02
#define E_PSHOT 	0x04
#define E_TRACT 	0x08
#define NOENEMY (struct Enemy *) -1

struct Enemy {
    int e_info;
    int e_dist;
    u_char e_course;	/* course to enemy */
    u_char e_edir;	/* enemy's current heading */
    u_char e_tcourse;	/* torpedo intercept course to enemy */
    u_int e_flags;
    int e_tractor;		/* who he's tractoring/pressoring */
    int e_phrange;		/* his phaser range */
    u_int e_hisflags;	/* his pflags. bug fix: 6/24/92 TC */
};

static int avoidTime;

#define MESSFUSEVAL 200		/* maint: removed trailing ';' 6/22/92 TC */
static int messfuse = MESSFUSEVAL;
static char rmessage[110];
static char tmessage[110];
static char *rmessages[] = {
    "01101010 01010101 01111010 10110101 01010101 01011110!",
    "Crush, Kill, DESTROY!!",
    "Run coward!",
    "Hey!  Come on you Hoser!  I dare you!",
    "Resistance is futile!",
    "Dry up and blow away you weenie!",
    "Go ahead, MAKE MY DAY!",
    "Just hit '<shift> Q' ... and spare both of us the trouble",
    "I have you now!",
    "By the way, any last requests?",
    "Dropping your shields is considered a peaceful act.",
    "Drop your shields, and I MAY let you live ...",
    "Don't you have midterms coming up or something?",
    "Ya wanna Tango baby?",
    "Hey! Outta my turf you filthy $t!",
    "I seeeee you...",
    "Booo!!!",
    "I claim this space for the $f.",
    "Give up fool ... you're meat.",
    "And another one bites the dust ...",
    "Use the force: Close your eyes and fight me.",
    "Surrender ... NOW!",
    "Hey!  Have you heard about the clever $t?  No?.. Me neither.",
    "You have been registered for termination.",
    "This'll teach you to mind your own business!",
    "Man, you stupid $ts never learn!",
    "ALL RIGHT, enough goofing off ... you're toast buster!",
    "You pesky humans just NEVER give up, do you?",
    "You're actually paying money to this server so you can play this game?",
    "My job is to put over-zealous weenies like you out of your misery.",
    "How can you stand to be $T?!?  What a hideous life!",
    "All $ts are losers!",
    "You're not running that ship on Microsoft Windows are you?",
    "$ts all suck.  Come join the $f!",
    "The $f shall crush all filthy $ts like you!"
};

char* termie_messages[] = {
    "Ha! Ha! I'm coming to take you away!",
    "I'm gonna DOOSH you!",
    "$n:  Hasta la vista.",
    "Come quietly, or there will be trouble. [thump] [thump]",
    "Make your peace with GOD.",
    "$n:  Say your prayers",
    "This galaxy isn't big enough for the two of us, $n.",
    "$n, I have a warrant for your arrest.",
    "$n, self-destruct now.  You have 20 seconds to comply.",
    "Coming through.  Out of my way.",
    "You feel lucky, punk?",
    "Killing is our business.  Business is good.",
    "$n: duck."
};

extern int debug;
extern int hostile;
extern int sticky;
extern int practice;
extern int cloaker;		/* robotII.c 10/15/94 [007] */
extern int polymorphic;

extern int target;		/* robotII.c 7/27/91 TC */
extern int phrange;		/* robotII.c 7/31/91 TC */
extern int trrange;		/* robotII.c 8/2/91 TC */

extern int quiet;		/* if set, avoid nuisance messaging */

extern int dogslow;		/* robotII.c 8/9/91 TC */
extern int dogfast;
extern int runslow;
extern int runfast;
extern int closeslow;
extern int closefast;

u_char	getcourse();
char *robo_message();
char *termie_message();		/* added 8/2/91 TC */
void exitRobot();
int phaser_plasmas();
void go_home(struct Enemy *ebuf);
int do_repair();
int isTractoringMe(struct Enemy *enemy_buf);
int projectDamage(int eNum, int *dirP);

char roboname[15];


void rmove()
{
    register struct player *j;
    register int i;
    register int burst;
    register int numHits, tDir;
    int		avDir;
    extern struct Enemy *get_nearest();
    struct Enemy *enemy_buf;
    struct player *enemy = NULL;
    static int	roboclock = 0;
    static int	avoid[2] = { -32, 32 };
    int no_cloak;
    char towhom[MSG_LEN];
    int timer;
    static int lastTorpped = 0; /* when we last fired a torp 4/13/92 TC */

    roboclock++;

    /* keep ghostbuster away */
    me->p_ghostbuster = 0;

    /* Check that I'm alive */
    if (me->p_status == PEXPLODE) {
        if (debug) ERROR(1,("Robot: Augh! exploding.\n"));
	return;
    }
    else if (me->p_status == PDEAD) {
	if (me->p_ntorp > 0)
               return;
	if (debug) ERROR(1,("Robot: done exploding and torps are gone.\n"));
	exitRobot();
	return;
    }


    timer=0;
    for (i = 0, j = &players[i]; i < (MAXPLAYER - TESTERS); i++, j++) {
	if ((j->p_status != PFREE) && !(j->p_flags & PFROBOT))
	    timer=1;
    }
    if (!timer && !sticky) {
	exitRobot();
	return;
    }

    /* if I'm a Terminator, quit if he quits, and quit if he dies and */
    /* I'm not "sticky" (-s) */

    if (target >= 0) {
	if (players[target].p_status == PFREE) { /* he went away */
	    me->p_status = PEXPLODE;	    
	    return;
	}
	if ((!sticky) && (players[target].p_status != PALIVE)) { /* he died */
	    me->p_status = PEXPLODE;
	    return;
	}
    }

    /* If it's been BOREDOM_TIME updates since we fired a torp, become
       hostile to all races, if we aren't already, and if we're not
       a practice robot (intended for guardian bots). 4/13/92 TC */

    if ((roboclock - lastTorpped > BOREDOM_TIME) && (!practice) && (!hostile) &&
        (me->p_team != 0 && !quiet)) {
        messAll(me->p_no,roboname,"I'm bored.");
        hostile++;
        declare_war(ALLTEAM, 0);
    }

    /* Our first priority is to phaser plasma torps in nearby vicinity... */
    /* If we fire, we aren't allowed to cloak... */
    no_cloak = phaser_plasmas();

    /* Find an enemy */

    enemy_buf = get_nearest();

    if ((enemy_buf != NULL) && (enemy_buf != NOENEMY)) {	/* Someone to kill */
	enemy = &players[enemy_buf->e_info];
	if (((random() % messfuse) == 0) &&
	    (hypot((double) me->p_x-enemy->p_x, (double) me->p_y-enemy->p_y) < 20000.0)) {
	    /* change 5/10/21 TC ...neut robots don't message */
	    messfuse = MESSFUSEVAL;
	    if (me->p_team != 0 && !quiet) {
		sprintf(towhom, " %s->%s",
			players[me->p_no].p_mapchars,
			players[enemy->p_no].p_mapchars);
		pmessage2(enemy->p_no, MINDIV, towhom, me->p_no, "%s",
			robo_message(enemy));
	    }
	    else if (target >= 0 && !quiet) {
		messAll(me->p_no,roboname,"%s",termie_message(enemy));
	    }
	}
	else
	    if (--messfuse == 0)
		messfuse = 1;
	timer = 0;
/*	if (debug)
	    ERROR(1,( "%d) noticed %d\n", me->p_no, enemy_buf->e_info));*/
    }
    else if (enemy_buf == NOENEMY) { /* no more players. wait 1 minute. */
	if (do_repair()) {
	    return;
	}
	go_home(0);
/*	if (debug)
	    ERROR(1,( "%d) No players in game.\n", me->p_no));*/
	return;
    }
    else if (enemy_buf == 0) {	 /* no one hostile */
/*	if (debug)
	    ERROR(1,( "%d) No hostile players in game.\n", me->p_no));*/
	if (do_repair()) {
	    return;
	}
	go_home(0);
	timer = 0;
	return;
    }

/* Note a bug in this algorithm:
** Once someone dies, he is forgotten.  This makes robots particularly easy
**  to kill on a suicide run, where you aim to where you think he will turn 
**  as you die.  Once dead, the robot will ignore you and all of your 
**  active torpedoes!
**/

/* Algorithm:
** We have an enemy.
** First priority: shoot at target in range.
** Second: Dodge torps and plasma torps.
** Third: Get away if we are damaged.
** Fourth: repair.
** Fifth: attack.
*/

/*
** If we are a practice robot, we will do all but the second.  One
** will be modified to shoot poorly and not use phasers.
**/
    /* Fire weapons!!! */
    /*
    ** get_nearest() has already determined if torpedoes and phasers
    ** will hit.  It has also determined the courses which torps and
    ** phasers should be fired.  If so we will go ahead and shoot here.
    ** We will lose repair and cloaking for the rest of this interrupt.
    ** if we fire here.
    */

    if (practice) {
	no_cloak = 1;
	if (enemy_buf->e_flags & E_TSHOT) {
/*	    if (debug)
		ERROR(1,( "%d) firing torps\n", me->p_no));*/
	    for (burst = 0; (burst < 3) && (me->p_ntorp < MAXTORP); burst++) {
		ntorp(enemy_buf->e_tcourse, TWOBBLE | TOWNERSAFE | TDETTEAMSAFE | TPRACTICE);
	    }
	}
    }
    else {
	if (enemy_buf->e_flags & E_TSHOT) {
/*	    if (debug)
		ERROR(1,( "%d) firing torps\n", me->p_no));*/
	    for (burst = 0; (burst < 2) && (me->p_ntorp < MAXTORP); burst++) {
		repair_off();
		if (! cloaker) cloak_off();
		ntorp(enemy_buf->e_tcourse, TWOBBLE | TOWNERSAFE | TDETTEAMSAFE);
		no_cloak++;
                lastTorpped = roboclock; /* record time of firing 4/13/92 TC */
	    }
	}
	if (enemy_buf->e_flags & E_PSHOT) {
/*	    if (debug)
		ERROR(1,( "%d) phaser firing\n", me->p_no));*/
	    no_cloak++;
	    repair_off();
	    if (! cloaker) cloak_off();
	    phaser(enemy_buf->e_course);
	}
    }

    /* auto pressor 7/27/91 TC */
    /* tractor/pressor rewritten on 5/1/92... glitches galore :-| TC */

    /* whoa, too close for comfort, or he's tractoring me, or
       headed in for me, or I'm hurt */

    /* a little tuning -- 0.8 on phrange and +/- 90 degrees in for pressor */

    /* pressor_player(-1); this didn't do anything before, so we'll let
       the pressors disengage by themselves 5/1/92 TC */
    if (enemy_buf->e_flags & E_TRACT) {	/* if pressorable */
	if (((enemy_buf->e_dist < 0.8 * enemy_buf->e_phrange) &&
	     (angdist(enemy_buf->e_edir, enemy_buf->e_course) > 64)) ||
	    (isTractoringMe(enemy_buf)) ||
	    (me->p_damage > 0)) {
	    if (!(enemy->p_flags & PFCLOAK)) {
		  if (debug)
		    ERROR(1,( "%d) pressoring %d\n", me->p_no,
			    enemy_buf->e_info));
		  pressor_player(enemy->p_no);
		  no_cloak++;
		  repair_off();
		  if (!cloaker) cloak_off();
	    }
	}
    }

    /* auto tractor 7/31/91 TC */

    /* tractor if not pressoring and... */

    /* tractor if: in range, not too close, and not headed +/- 90 degrees */
    /* of me, and I'm not hurt */
    
    if ((!(me->p_flags & PFPRESS)) &&
	(enemy_buf->e_flags & E_TRACT) &&
	(angdist(enemy_buf->e_edir, enemy_buf->e_course) < 64) &&
	(enemy_buf->e_dist > 0.7 * enemy_buf->e_phrange)) {
	if (!(me->p_flags & PFTRACT)) {
	    if (debug)
		ERROR(1,( "%d) tractoring %d\n", me->p_no,
			enemy_buf->e_info));
	    tractor_player(enemy->p_no);
	    no_cloak++;
	}
    }	
    else
	tractor_player(-1);	/* otherwise don't tractor */

    /* Avoid torps */
    /*
    ** This section of code allows robots to avoid torps.
    ** Within a specific range they will check to see if
    ** any of the 'closest' enemies torps will hit them.
    ** If so, they will evade for four updates.
    ** Evading is all they will do for this round, other than shooting.
    */

    if (!practice) {
	if ((enemy->p_ntorp < 5)) {
	    if ((enemy_buf->e_dist < 15000) || (avoidTime > 0)) {
		numHits = projectDamage(enemy->p_no, &avDir);
		if (debug) {
		    ERROR(1,( "%d hits expected from %d from dir = %d\n",
			    numHits, enemy->p_no, avDir));
		}
		if (numHits == 0) {
		    if (--avoidTime > 0) {	/* we may still be avoiding */
			if (angdist(me->p_desdir, me->p_dir) > 64)
			    me->p_desspeed = dogslow;
			else
			    me->p_desspeed = dogfast;
			return;
		    }
		} else {
		    /*
		     * Actually avoid Torps
		     */ 
		    avoidTime = AVOID_TIME;
		    tDir = avDir - me->p_dir;
		    /* put into 0->255 range */
		    tDir = NORMALIZE(tDir);
		    if (debug)
			ERROR(1,( "mydir = %d avDir = %d tDir = %d q = %d\n",
			    me->p_dir, avDir, tDir, tDir / 64));
		    switch (tDir / 64) {
		    case 0:
		    case 1:
			    set_course(NORMALIZE(avDir + 64));
			    break;
		    case 2:
		    case 3:
			    set_course(NORMALIZE(avDir - 64));
			    break;
		    }
		    if (!no_cloak)
			cloak_on();

		    if (angdist(me->p_desdir, me->p_dir) > 64)
			me->p_desspeed = dogslow;
		    else
			me->p_desspeed = dogfast;
			
		    shield_up();
		    detothers();	/* hmm */
		    if (debug)
			ERROR(1,( "evading to dir = %d\n", me->p_desdir));
		    return;
		}
	    }
	}

	/*
	** Trying another scheme.
	** Robot will keep track of the number of torps a player has
	** launched.  If they are greater than say four, the robot will
	** veer off immediately.  Seems more humanlike to me.
	*/

	else if (enemy_buf->e_dist < 15000) {
	    if (--avoidTime > 0) {	/* we may still be avoiding */
		if (angdist(me->p_desdir, me->p_dir) > 64)
		    me->p_desspeed = dogslow;
		else
		    me->p_desspeed = dogfast;
		return;
	    }
	    if (random() % 2) {
		me->p_desdir = NORMALIZE(enemy_buf->e_course - 64);
		avoidTime = AVOID_TIME;
	    }
	    else {
		me->p_desdir = NORMALIZE(enemy_buf->e_course + 64);
		avoidTime = AVOID_TIME;
	    }
	    if (angdist(me->p_desdir, me->p_dir) > 64)
		me->p_desspeed = dogslow;
	    else
		me->p_desspeed = dogfast;
	    shield_up();
	    return;
	}
    }
	    
    /* Run away */
    /*
    ** The robot has taken damage.  He will now attempt to run away from
    ** the closest player.  This obviously won't do him any good if there
    ** is another player in the direction he wants to go.
    ** Note that the robot will not run away if he dodged torps, above.
    ** The robot will lower his shields in hopes of repairing some damage.
    */

#define STARTDELTA 5000		/* ships appear +/- delta of home planet */

    if (me->p_damage > 0 && enemy_buf->e_dist < 13000) {
	if (me->p_etemp > 900)		/* 90% of 1000 */
	    me->p_desspeed = runslow;
	else
	    me->p_desspeed = runfast;
	if (!no_cloak)
	    cloak_on();
	repair_off();
	shield_down();
	set_course(enemy_buf->e_course - 128);
	if (debug)
	    ERROR(1,( "%d(%d)(%d/%d) running from %c%d %16s damage (%d/%d) dist %d\n",
		me->p_no,
		(int) me->p_kills,
		me->p_damage,
		me->p_shield,
		teamlet[enemy->p_team],
		enemy->p_no,
		enemy->p_login,
		enemy->p_damage,
		enemy->p_shield,
		enemy_buf->e_dist));
	return;
    }

    /* Repair if necessary (we are safe) */
    /*
    ** The robot is safely away from players.  It can now repair in peace.
    ** It will try to do so now.
    */

    if (do_repair()) {
	return;
    }

    /* Attack. */
    /*
    ** The robot has nothing to do.  It will check and see if the nearest
    ** enemy fits any of its criterion for attack.  If it does, the robot
    ** will speed in and deliver a punishing blow.  (Well, maybe)
    */

    if ((enemy_buf->e_flags & E_INTRUDER) || (enemy_buf->e_dist < 15000)
	|| (hostile)) {
	if ((!no_cloak) && (enemy_buf->e_dist < 10000))
	    cloak_on();
	shield_up();
/*	if (debug)
	    ERROR(1,( "%d(%d)(%d/%d) attacking %c%d %16s damage (%d/%d) dist %d\n",
		me->p_no,
		(int) me->p_kills,
		me->p_damage,
		me->p_shield,
		teamlet[enemy->p_team],
		enemy->p_no,
		enemy->p_login,
		enemy->p_damage,
		enemy->p_shield,
		enemy_buf->e_dist));*/

	if (enemy_buf->e_dist < 15000) {
	    set_course(enemy_buf->e_course + 
		    avoid[(roboclock / AVOID_CLICKS) % SIZEOF(avoid)]);
	    if (angdist(me->p_desdir, me->p_dir) > 64)
		set_speed(closeslow);
	    else
		set_speed(closefast);
	}
	else {
	    me->p_desdir = enemy_buf->e_course;
	    if (angdist(me->p_desdir, me->p_dir) > 64)
		set_speed(closeslow);
	    if (target >= 0)	/* 7/27/91 TC */
		set_speed(12);
	    else if (me->p_etemp > 900)		/* 90% of 1000 */
		set_speed(runslow);
	    else
		set_speed(runfast);
	}
    }
    else {
	go_home(enemy_buf);
    }
}

u_char getcourse(x, y)
int x, y;
{
	return((u_char) rint((atan2((double) (x - me->p_x),
	    (double) (me->p_y - y)) / 3.14159 * 128.)));
}

struct {
    int x;
    int y;
} center[] = { {GWIDTH/2, GWIDTH/2}, /* change 5/20/91 TC was {0,0} */
		{GWIDTH / 4, GWIDTH * 3 / 4},		/* Fed */
		{GWIDTH / 4, GWIDTH / 4},		/* Rom */
		{0, 0},
		{GWIDTH * 3 / 4, GWIDTH  / 4},		/* Kli */
		{0, 0},
		{0, 0},
		{0, 0},
		{GWIDTH * 3 / 4, GWIDTH * 3 / 4}};	/* Ori */

/* This function means that the robot has nothing better to do.
   If there are hostile players in the game, it will try to get
   as close to them as it can, while staying in its own space.
   Otherwise, it will head to the center of its own space.
*/
void go_home(struct Enemy *ebuf)
{
    int x, y;
    double dx, dy;
    struct player *j;

    if (ebuf == 0) {  /* No enemies */
/*	if (debug)
	    ERROR(1,( "%d) No enemies\n", me->p_no));*/
	x = center[me->p_team].x;
	y = center[me->p_team].y;
    }
    else {	/* Let's get near him */
	j = &players[ebuf->e_info];
	x = j->p_x;
	y = j->p_y;
	switch (me->p_team) {
	    case FED:
		if (x > (GWIDTH/2) - 5000)
		    x = (GWIDTH/2) - 5000;
		if (y < (GWIDTH/2) + 5000)
		    y = (GWIDTH/2) + 5000;
		break;
	    case ROM:
		if (x > (GWIDTH/2) - 5000)
		    x = (GWIDTH/2) - 5000;
		if (y > (GWIDTH/2) - 5000)
		    y = (GWIDTH/2) - 5000;
		break;
	    case KLI:
		if (x < (GWIDTH/2) + 5000)
		    x = (GWIDTH/2) + 5000;
		if (y > (GWIDTH/2) - 5000)
		    y = (GWIDTH/2) - 5000;
		break;
	    case ORI:
		if (x < (GWIDTH/2) + 5000)
		    x = (GWIDTH/2) + 5000;
		if (y < (GWIDTH/2) + 5000)
		    y = (GWIDTH/2) + 5000;
		break;
	}
    }
/*    if (debug)
	ERROR(1,( "%d) moving towards (%d/%d)\n",
	    me->p_no, x, y));*/

    /* Note that I've decided that robots should never stop moving.
    ** It makes them too easy to kill
    */

    me->p_desdir = getcourse(x, y);
    if (angdist(me->p_desdir, me->p_dir) > 64)
	set_speed(dogslow);
    else if (me->p_etemp > 900)		/* 90% of 1000 */
	set_speed(runslow);
    else {
	dx = x - me->p_x;
	dy = y - me->p_y;
	set_speed(((int) hypot(dx, dy) / 5000) + 3); /* was missing (int) 5/1/92 TC */
    }
    if (! cloaker) cloak_off();
}

int phaser_plasmas()
{
  struct torp *t;
  int myphrange;

  myphrange = phrange; /*PHASEDIST * me->p_ship.s_phaserdamage / 100;*/
  for (t=firstPlasma; t<=lastPlasma; t++) {
    if (t->t_status != TMOVE) 
      continue;
    if (t->t_owner == me->p_no) 
      continue;
    if (!(t->t_war & me->p_team) && !(me->p_war & t->t_team)) 
      continue;
    if (abs(t->t_x - me->p_x) > myphrange) 
      continue;
    if (abs(t->t_y - me->p_y) > myphrange) 
      continue;
    if (myphrange < hypot((float) t->t_x - me->p_x, (float) t->t_y - me->p_y))
      continue;
    repair_off();
    if (! cloaker) 
      cloak_off();
    phaser(getcourse(t->t_x, t->t_y));
    return 1;
    break;
  }
  return 0;		
}

int projectDamage(int eNum, int *dirP)
{
	register int		i, j, numHits = 0, mx, my, tx, ty, dx, dy;
	double			tdx, tdy, mdx, mdy;
	register struct torp	*t;

	*dirP = 0;
	for (i = 0, t = &torps[eNum * MAXTORP]; i < MAXTORP; i++, t++) {
		if (t->t_status == TFREE)
			continue;
		tx = t->t_x; ty = t->t_y;
		mx = me->p_x; my = me->p_y;
		tdx = (double) t->t_gspeed * Cos[t->t_dir];
		tdy = (double) t->t_gspeed * Sin[t->t_dir];
		mdx = (double) me->p_speed * Cos[me->p_dir] * WARP1;
		mdy = (double) me->p_speed * Sin[me->p_dir] * WARP1;
		for (j = t->t_fuse; j > 0; j--) {
			tx += tdx; ty += tdy;
			mx += mdx; my += mdy;
			dx = tx - mx; dy = ty - my;
			if (ABS(dx) < EXPDIST && ABS(dy) < EXPDIST) {
				numHits++;
				*dirP += t->t_dir;
				break;
			}
		}
	}
	if (numHits > 0)
		*dirP /= numHits;
	return numHits;
}

int isTractoringMe(struct Enemy *enemy_buf)
{
    return ((enemy_buf->e_hisflags & PFTRACT) && /* bug fix: was using */
	    !(enemy_buf->e_hisflags & PFPRESS) && /* e_flags 6/24/92 TC */
	    (enemy_buf->e_tractor == me->p_no));
}

struct Enemy ebuf;

struct Enemy *
get_nearest()
{
    int pcount = 0;
    register int i;
    register struct player *j;
    int intruder = 0;
    int tdist;
    double dx, dy;
    double vxa, vya, l;	/* Used for trap shooting */
    double vxt, vyt;
    double vxs, vys;
    double dp;

    /* Find an enemy */
    ebuf.e_info = me->p_no;
    ebuf.e_dist = GWIDTH + 1;

    pcount = 0;  /* number of human players in game */

    if (target >= 0) {
	j = &players[target];
	if (!(me->p_war & j->p_team))
	    declare_war(players[target].p_team, 0); /* make sure we're at war 7/31/91 TC */

	/* We have an enemy */
	/* Get his range */
	dx = j->p_x - me->p_x;
	dy = j->p_y - me->p_y;
	tdist = hypot(dx, dy);

	if (j->p_status != POUTFIT) { /* ignore target if outfitting */
	    ebuf.e_info = target;
	    ebuf.e_dist = tdist;
	    ebuf.e_flags &= ~(E_INTRUDER);
	}
	
	/* need a loop to find hostile ships */
	/* within tactical range */

	for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	    if ((j->p_status != PALIVE) || (j == me) ||
		((j->p_flags & PFROBOT) && (!hostile)))
		continue;
	    else
		pcount++;	/* Other players in the game */
	    if ((j->p_war & me->p_team) ||
		(me->p_war & j->p_team)) {
		/* We have an enemy */
		/* Get his range */
		dx = j->p_x - me->p_x;
		dy = j->p_y - me->p_y;
		tdist = hypot(dx, dy);
		
		/* if target's teammate is too close, mark as nearest */

		if ((tdist < ebuf.e_dist) && (tdist < 15000)) {
		    ebuf.e_info = i;
		    ebuf.e_dist = tdist;
		    ebuf.e_flags &= ~(E_INTRUDER);
		}
	    }			/* end if */
	}			/* end for */
    }
    else {			/* no target */
	/* avoid dead slots, me, other robots (which aren't hostile) */
	for (i = 0, j = &players[i]; i < MAXPLAYER; i++, j++) {
	    if ((j->p_status != PALIVE) || (j == me) ||
		((j->p_flags & PFROBOT) && (!hostile)) ||
                (j->p_flags & PFPRACTR))
		continue;
	    else
		pcount++;	/* Other players in the game */
	    if ((j->p_war & me->p_team) || 
		(me->p_war & j->p_team)) {
		/* We have an enemy */
		/* Get his range */
		dx = j->p_x - me->p_x;
		dy = j->p_y - me->p_y;
		tdist = hypot(dx, dy);
		
		/* Check to see if ship is in our space. */
		switch (me->p_team) {
		  case FED:
		    intruder = ((j->p_x < GWIDTH/2) && (j->p_y > GWIDTH/2));
		    break;
		  case ROM:
		    intruder = ((j->p_x < GWIDTH/2) && (j->p_y < GWIDTH/2));
		    break;
		  case KLI:
		    intruder = ((j->p_x > GWIDTH/2) && (j->p_y < GWIDTH/2));
		    break;
		  case ORI:
		    intruder = ((j->p_x > GWIDTH/2) && (j->p_y > GWIDTH/2));
		    break;
		}
		
		if (tdist < ebuf.e_dist) {
		    ebuf.e_info = i;
		    ebuf.e_dist = tdist;
		    if (intruder)
			ebuf.e_flags |= E_INTRUDER;
		    else
			ebuf.e_flags &= ~(E_INTRUDER);
		}
	    }			/* end if */
	}			/* end for */
    }				/* end else */
    if (pcount == 0) {
	return NOENEMY;    /* no players in game */
    } else if (ebuf.e_info == me->p_no) {
	return 0;			/* no hostile players in the game */
    } else {
	j = &players[ebuf.e_info];

	/* Get torpedo course to nearest enemy */
	ebuf.e_flags &= ~(E_TSHOT);

	vxa = (j->p_x - me->p_x);
	vya = (j->p_y - me->p_y);
	l = hypot(vxa, vya);	/* Normalize va */
	if (l!=0) {
	    vxa /= l;
	    vya /= l;
	}
	vxs = (Cos[j->p_dir] * j->p_speed) * WARP1;
	vys = (Sin[j->p_dir] * j->p_speed) * WARP1;
	dp = vxs * vxa + vys * vya;	/* Dot product of (va vs) */
	dx = vxs - dp * vxa;
	dy = vys - dp * vya;
	l = hypot(dx, dy);      /* Determine how much speed is required */
        dp = (float) (me->p_ship.s_torpspeed * WARP1);
        l = (dp * dp - l * l);
        if (l > 0) {
	    double he_x, he_y, area;

	    /* Only shoot if within distance */
	    he_x = j->p_x + Cos[j->p_dir] * j->p_speed * 50 * WARP1;
	    he_y = j->p_y + Sin[j->p_dir] * j->p_speed * 50 * WARP1;
	    area = 50 * me->p_ship.s_torpspeed * WARP1;
	    if (hypot(he_x - me->p_x, he_y - me->p_y) < area) {
		l=sqrt(l);
		vxt = l * vxa + dx;
		vyt = l * vya + dy;
		ebuf.e_flags |= E_TSHOT;
		ebuf.e_tcourse = getcourse((int) vxt + me->p_x, (int) vyt + me->p_y);
	    }
	}

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


	/* get his phaser range */
	ebuf.e_phrange = PHASEDIST * j->p_ship.s_phaserdamage / 100;

	/* get course info */
	ebuf.e_course = getcourse(j->p_x, j->p_y);
	ebuf.e_edir = j->p_dir;
	ebuf.e_hisflags = j->p_flags;
	ebuf.e_tractor = j->p_tractor;
	/*
	if (debug)
	    ERROR(1,( "Set course to enemy is %d (%d)\n",
		    (int)ebuf.e_course, 
		    (int) ebuf.e_course * 360 / 256));
	*/

	/* check to polymorph */

	if ((polymorphic) && (j->p_ship.s_type != me->p_ship.s_type) &&
	    (j->p_ship.s_type != ATT)) { /* don't polymorph to ATT 4/8/92 TC */
	    extern int config();
	    int old_shield;
	    int old_damage;
	    old_shield = me->p_ship.s_maxshield;
	    old_damage = me->p_ship.s_maxdamage;
	    
	    getship(&(me->p_ship), j->p_ship.s_type);
	    config();
	    if (me->p_speed > me->p_ship.s_maxspeed)
		me->p_speed = me->p_ship.s_maxspeed;
	    me->p_shield = (me->p_shield * (float)me->p_ship.s_maxshield)
		/(float)old_shield;
	    me->p_damage = (me->p_damage * (float)me->p_ship.s_maxdamage)
		/(float)old_damage;
	}

	return &ebuf;
    }
}

struct planet* get_nearest_planet()
{
    register int i;
    register struct planet *l;
    register struct planet *nearest;
    int dist = GWIDTH;		/* Manhattan distance to nearest planet */
    int ldist;

    nearest = &planets[0];
    for (i = 0, l = &planets[i]; i < MAXPLANETS; i++, l++) {
	if ((ldist = (abs(me->p_x - l->pl_x) + abs(me->p_y - l->pl_y))) <
	    dist) {
	    dist = ldist;
	    nearest = l;
	}
    }
    return nearest;
}

int do_repair()
{
/* Repair if necessary (we are safe) */

    struct planet* l;
    int dx, dy;
    int dist;

    l = get_nearest_planet();
    dx = abs(me->p_x - l->pl_x);
    dy = abs(me->p_y - l->pl_y);

    if (me->p_damage > 0 || me->p_fuel < 2000) {
	if (me->p_war & l->pl_owner) {
	    if (l->pl_armies > 0) {
		if ((dx < PFIREDIST) && (dy < PFIREDIST)) {
		    if (debug)
			ERROR(1,( "%d) on top of hostile planet (%s)\n", me->p_no, l->pl_name));
		    return 0;	/* can't repair on top of hostile planets */
		}
		if ((int) (hypot((double) dx, (double) dy)) < PFIREDIST) {
		    if (debug)
		        ERROR(1,("%d) on top of hostile planet (%s)\n", 
				me->p_no, l->pl_name));
		    return 0;
		}
	    } 
	    me->p_desspeed = 0;
	}
	else { /* if friendly */
	    if ((l->pl_flags & PLREPAIR) &&
		!(me->p_flags & PFORBIT)) { /* oh, repair! */
		dist = (hypot((double) dx, (double) dy));

		if (debug)
		    ERROR(1,( "%d) locking on to planet %d\n",
			    me->p_no, l->pl_no));
		if (! cloaker) cloak_off();
		shield_down();
		me->p_desdir = getcourse(l->pl_x, l->pl_y);
		lock_planet(l->pl_no);
		me->p_desspeed = 4;
		if (dist-(ORBDIST/2) < (11500 * me->p_speed * me->p_speed) /
		    me->p_ship.s_decint) {
		    if (me->p_desspeed > 2) {
			set_speed(2);
		    }
		}

		if ((dist < ENTORBDIST) && (me->p_speed <= 2))  {
		    me->p_flags &= ~PFPLLOCK;
		    orbit();
		}
		return 1;
	    }
	    else {			/* not repair, so ignore it */
		me->p_desspeed = 0;
	    }
	}
	shield_down();
	if (me->p_speed == 0)
	    repair();
	if (debug)
	    ERROR(1,( "%d) repairing damage at %d\n",
		me->p_no,
		me->p_damage));
	return 1;
    }
    else {
	return 0;
    }
}

char *robo_message(enemy)
struct player *enemy;
{
    int i;
    char *s,*t;
    
    i=(random() % (sizeof(rmessages)/sizeof(rmessages[0])));

    for (t=rmessages[i], s=rmessage; *t!='\0'; s++, t++) {
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
		case 'f':
		    switch (me->p_team) {
		    case FED:
			strcpy(s, "Federation");
			break;
		    case ROM:
			strcpy(s, "Romulan empire");
			break;
		    case KLI:
			strcpy(s, "Klingon empire");
			break;
		    case ORI:
			strcpy(s, "Orion empire");
			break;
		    }
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
    return rmessage;
}

char* termie_message(enemy)
struct player* enemy;
{
    int i;
    char *s,*t;
    
    i=(random() % (sizeof(termie_messages)/sizeof(termie_messages[0])));

    for (t=termie_messages[i], s=tmessage; *t!='\0'; s++, t++) {
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
		case 'f':
		    switch (me->p_team) {
		    case FED:
			strcpy(s, "Federation");
			break;
		    case ROM:
			strcpy(s, "Romulan empire");
			break;
		    case KLI:
			strcpy(s, "Klingon empire");
			break;
		    case ORI:
			strcpy(s, "Orion empire");
			break;
		    }
		    s=s+strlen(s)-1;
		    break;
		case 'n':	/* name 8/2/91 TC */
		    strcpy(s, enemy->p_name);
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
    return tmessage;
}

void exitRobot()
{
    if (me != NULL && me->p_team != ALLTEAM) {
        if (target >= 0 && !quiet) {
            messAll(me->p_no,roboname, "I'll be back.");
        } else {
            messAll(me->p_no,&roboname[0],
                    "%s %s (%s) leaving the game (%.16s@%.16s)",
                    ranks[me->p_stats.st_rank].name,
                    me->p_name,
                    me->p_mapchars,
                    me->p_login,
                    me->p_monitor);
        }
    }

    freeslot(me);
    return;
}

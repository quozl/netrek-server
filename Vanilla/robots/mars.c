/*
     mars.c
 */

/*
  Modified for dogfight overseeing by Jeff Nelson (jn).
  Based on puck.c of 2.01pl5 on Jan 7 1993.

  Modified for use with guzzler code by Michael Kantner (MK)
  Base upon code ftp'd from hobbes.ksu.ksu.edu on 9-9-93
  Please mail all bugs, patches, comments to kantner@hot.caltech.edu
*/

#include "copyright.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "alarm.h"
#include "roboshar.h"
#include "marsdefs.h"
#include "util.h"
#include "slotmaint.h"

#define DOG_STATS	".dog_players"

char DogStats[256];

extern int redrawall;
extern int lastm;

char *roboname="Mars";

/* lots of neat flags */
int debug;
int oldmctl=0;			/* was "level", for monitoring messages */
int practice;			/* don't interfere with live game */
int length = 9999999;		/* Number of games to play */
struct planet *oldplanets;	/* for restoring galaxy */

int target;			/* Terminator's target 7/27/91 TC */
int phrange;			/* phaser range 7/31/91 TC */
int trrange;			/* tractor range 8/2/91 TC */

/* velocities 8/9/91 TC */

int dogslow;			/* dodge speed (hard turn) */
int dogfast;			/* dodge speed (soft turn) */
int runslow;			/* run speed (flat etemp) */
int runfast;			/* run speed (gain etemp) */
int closeslow;			/* approach speed (hard turn) */
int closefast;			/* approach speed (soft turn) */

void cleanup();
void config();

void greeting()
{
	printf("****************************************\n");
	printf("                  WARNING!!\n");
	printf(" This version of the dog-fight robot can\n");
	printf("not save player stats. This is currently\n");
	printf("in development. Any suggestions on which\n");
	printf("stats you would like to see saved can be\n");
	printf("sent to: server@ecst.csuchico.edu\n");
	printf("****************************************\n");
}

int main(argc, argv)
int argc;
char **argv;
{
    void marsmove();
    int team = 4;
    int pno;
    int class;			/* ship class 8/9/91 TC */

    getpath();                  /* added 11/6/92 DRG, 9/9/93 MK */    
    class = STARBASE;
    target = -1;		/* no target 7/27/91 TC */
    for( ; argc>1 && argv[1][0]=='-'; argc--,argv++) {
	switch(argv[1][1]) {
	  case 'd':
	    debug++;
	    break;
	  case 'p':  /* Used in marsmove() and keeps planet alignment here */
	    practice++;
	    break;
	  case 'l':		/* length of game */
	    if (argv[1][2] != '\0') {
		length = atoi(&argv[1][2]);
	    } else {
		length = 1;
	    }
	    break;
	  default:
            /*corrected the acceptable arg list*/
	    ERROR(1,("Unknown option '%c' (must be one of: dpl)\n", argv[1][1]));
	    exit(1);
	}			/* end switch argv[1][1] */
	
	
    }				/* end for */
    srandom(getpid() * time((long *) 0));
    openmem(1);
    readsysdefaults();

    sprintf(DogStats,"%s/%s",LOCALSTATEDIR,DOG_STATS);
	
    if ( (pno = pickslot(QU_ROBOT)) < 0) exit(0);
    me = &players[pno];
    myship = &me->p_ship;
    mystats = &me->p_stats;
    lastm = mctl->mc_current;
    /* At this point we have memory set up.  If we aren't a fleet, we don't
       want to replace any other robots on this team, so we'll check the
       other players and get out if there are any on our team.
    */

    robonameset(me);                    /* Set the robot@nowhere */
    enter(team, 0, pno, class, "Mars"); /* was BATTLESHIP 8/9/91 TC */

    me->p_pos = -1;		     /* So robot stats don't get saved */
    me->p_flags |= PFROBOT;	     /* Mark as a robot */
    /* displace to on overlooking position */
    /* maybe we should just make it fight? */
    p_x_y_set(me, 50000, 5000);

    alarm_init();
    if (!debug)
	signal(SIGINT, cleanup);

    oldplanets = (struct planet *) malloc(sizeof(struct planet) * MAXPLANETS);
    memcpy(oldplanets, planets, sizeof(struct planet) * MAXPLANETS);
    config();
    do_war();   /*added MK-9/20/92, to plague and fill slots immediately*/
    mars_rules();

    status->gameup |= GU_DOG;
    me->p_process = getpid();
    p_ups_set(me, 10 / HOWOFTEN);
    me->p_status = PALIVE;		/* Put robot in game */
    init_mars();
    while ((status->gameup) & GU_GAMEOK) {
        alarm_wait_for();
        marsmove();
    }
    cleanup();
    return 0;
}


void config()
{
    int i;
    FILE* f;
    char TournMap_File[256];

    /* calc class-specific stuff */

    phrange = PHASEDIST * me->p_ship.s_phaserdamage / 100;
    trrange = TRACTDIST * me->p_ship.s_tractrng;

    if (debug) printf("My phaser range: %d.\n", phrange);
    if (debug) printf("My tractor range: %d.\n", trrange);

    myship->s_turns = 2140000000;
    myship->s_accint = 10000;
    myship->s_decint = 10000;
    myship->s_warpcost = 1;
    myship->s_tractstr = 1;
    myship->s_torpdamage = -1;
    myship->s_plasmadamage = -1;
    myship->s_phaserdamage = -1;
    me->p_hostile = 0;
    me->p_swar = 0;
    me->p_war  = 0;
    me->p_team = 0;	/* indep */
    myship->s_type = STARBASE;
/*    myship->s_type = SCOUT;*/
    myship->s_mass = 32765;
    myship->s_repair = 30000;
    myship->s_maxspeed = 30;
    myship->s_wpncoolrate = 100;
    myship->s_egncoolrate = 100;
    myship->s_maxwpntemp = 10000;
    myship->s_maxegntemp = 10000;
    oldmctl = mctl->mc_current;	/* start watching messages */

    if (practice) return;	/* don't change galaxy */
    /* align planets */

#define IND 0

    sprintf(TournMap_File,"%s/%s",SYSCONFDIR,TOURNMAP);

    f = fopen(TournMap_File,"r");
    if (f == NULL) {
	fprintf(stderr,"%s: Tournament map file missing\n",TOURNMAP);
    } else {
        char dummy[3];
        int newx, newy;

	for (i = 0; i < 40; i++) {
		fscanf(f,"%s %d %d\n",dummy,&newx,&newy);
		planets[i].pl_x = newx;
		planets[i].pl_y = newy;
		planets[i].pl_owner = IND;
		planets[i].pl_info |= ALLTEAM;
		planets[i].pl_flags &= ~(PLFUEL | PLREPAIR | PLAGRI);
	}
    }
    fclose(f);

    planets[10].pl_flags |= (PLFUEL | PLREPAIR);
    planets[25].pl_flags |= (PLFUEL | PLREPAIR);
    planets[31].pl_flags |= (PLFUEL | PLREPAIR);
    planets[32].pl_flags |= (PLFUEL | PLREPAIR);
    planets[33].pl_flags |= (PLFUEL | PLREPAIR);
    planets[35].pl_flags |= (PLFUEL | PLREPAIR);
    planets[36].pl_flags |= (PLFUEL | PLREPAIR);
    planets[37].pl_flags |= (PLFUEL | PLREPAIR);
    planets[38].pl_flags |= (PLFUEL | PLREPAIR);

/*
  Everyone joins a dogfight server as ROM
  The robot tosses them to other teams at will
*/

    queues[QU_PICKUP].tournmask = ROM; 

#ifdef nodef
    planets[ 0].pl_couptime = 999999; /* no Feds */
    planets[20].pl_couptime = 999999; /* no Kli */
    planets[30].pl_couptime = 999999; /* no Ori */
#endif
}

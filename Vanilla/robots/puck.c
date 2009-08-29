 /* puck.c
 */

/*
  Modified for use with guzzler code by Michael Kantner (MK)
  Base upon code ftp'd from hobbes.ksu.ksu.edu on 9-9-93
  Please mail all bugs, patches, comments to kantner@alumni.caltech.edu
*/

#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "alarm.h"
#include "puckdefs.h"
#include "roboshar.h"
#include "puckmove.h"
#include "util.h"
#include "slotmaint.h"

#ifdef PUCK_FIRST 
#include <sys/sem.h> 

struct sembuf sem_puck_op[1]; 
int sem_puck;
#endif /*PUCK_FIRST*/

void config(int);

extern int redrawall;
extern int lastm;

char *roboname="Puck";

struct player *anncer;

/* lots of neat flags */
int debug;
int oldmctl;			/* was "level", for monitoring messages */
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

int main(argc, argv)
int argc;
char **argv;
{
    void rmove();
    int team = 4;
    int pno, anno;
    int class;			/* ship class 8/9/91 TC */
    int inhl = 0;

    getpath();                  /* added 11/6/92 DRG, 9/9/93 MK */    
    class = STARBASE;
    target = -1;		/* no target 7/27/91 TC */
    for( ; argc>1 && argv[1][0]=='-'; argc--,argv++) {
	switch(argv[1][1]) {
	case 'i':
	  inhl++;;
	  break;
	case 'd':
	  debug++;
	  break;
	case 'p':  /* Used in rmove() and keeps planet alignment here */
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
	  ERROR(1,("Unknown option '%c' (must be one of: idpl)\n", argv[1][1]));
	  exit(1);
	}			/* end switch argv[1][1] */
	
	
    }				/* end for */
    srandom(getpid() * time((long *) 0));
    openmem(1);
    readsysdefaults();
    if ( (pno = pickslot(QU_ROBOT)) < 0 ) exit(0);
    me = &players[pno];
    if ( (anno = pickslot(QU_ROBOT))< 0 ) {
	freeslot(me);
	exit(0);
    }
    anncer = &players[anno];

    myship = &me->p_ship;
    mystats = &me->p_stats;
    lastm = mctl->mc_current;
 
#ifdef PUCK_FIRST
    if((sem_puck = semget(PUCK_FIRST, 1, 0600)) == -1)
      {
	ERROR(1,("Puck unable to get semaphore."));
      }

    sem_puck_op[0].sem_num = 0;
    sem_puck_op[0].sem_op = 1;
    sem_puck_op[0].sem_flg = 0; 
#endif /*PUCK_FIRST*/

    /* At this point we have memory set up.  If we aren't a fleet, we don't
       want to replace any other robots on this team, so we'll check the
       other players and get out if there are any on our team.
    */

    robonameset(me);       /* Set the robot@nowhere fields */
    robonameset(anncer);
    enter(team, 0, pno, class, "Puck");       /* Create the puck */
    enter(team, 0, anno, class, "Announcer"); /* Create the announcer */

    me->p_pos = -1;			/* So robot stats don't get saved */
    me->p_flags |= PFROBOT;		/* Mark as a robot */
    anncer->p_pos = -1;			/* So robot stats don't get saved */
    anncer->p_flags |= PFROBOT;		/* Mark as a robot */

    alarm_init();
    if (!debug)
	signal(SIGINT, cleanup);

    oldplanets = (struct planet *) malloc(sizeof(struct planet) * MAXPLANETS);
    memcpy(oldplanets, planets, sizeof(struct planet) * MAXPLANETS);
    config(inhl);
    do_war();   /*added MK-9/20/92, to plague and fill slots immediately*/
    puck_rules();

    status->gameup |= GU_PUCK;
    me->p_process = getpid();
    p_ups_set(me, 10 / HOWOFTEN);
    anncer->p_process = 0;  /* Announcer will not be signalled by DS */

    me->p_status = PALIVE;		/* Put robot in game */
    anncer->p_status = PALIVE;
    do_faceoff();
    while ((status->gameup) & GU_GAMEOK) {
        alarm_wait_for();
        rmove();
    }
    cleanup();
    return 0;
}



void config(int inhl)
{
    int i;

    if (inhl == 0){
      /* Fix the wait queue so that only NUMSKATERS can play */
      queues[QU_PICKUP].free_slots -= ((MAXPLAYER - TESTERS) - NUMSKATERS);
      queues[QU_PICKUP].max_slots = NUMSKATERS;
      queues[QU_PICKUP].tournmask = KLI|ORI; /* Only allow ORI and ROM */
      queues[QU_PICKUP].low_slot  = 0;  
      queues[QU_PICKUP].high_slot = NUMSKATERS;

      queues[QU_PICKUP_OBS].free_slots += ((MAXPLAYER - TESTERS) + NUMSKATERS);
      queues[QU_PICKUP_OBS].max_slots  += ((MAXPLAYER - TESTERS) + NUMSKATERS);
      queues[QU_PICKUP_OBS].tournmask = KLI|ORI; /* Only allow ORI and ROM */
      queues[QU_PICKUP_OBS].low_slot  = NUMSKATERS;  
      queues[QU_PICKUP_OBS].high_slot = MAXPLAYER;
    }
    else{ /*INHL style play*/
      /* Turn off the pickup queues */
      queues[QU_PICKUP].q_flags     = 0;
      queues[QU_PICKUP_OBS].q_flags = 0;
      
      queues[QU_HOME].free_slots = 6; /* Off by default, inlbot gives 8 */
      queues[QU_HOME].max_slots  = 6;
      queues[QU_HOME].tournmask  = KLI;
      queues[QU_HOME].low_slot   = 0;
      queues[QU_HOME].high_slot  = 6;
      queues[QU_HOME].q_flags   |= QU_OPEN;

      queues[QU_AWAY].free_slots = 6; /* Off by default, inlbot gives 8 */
      queues[QU_AWAY].max_slots  = 6;
      queues[QU_AWAY].tournmask  = ORI;
      queues[QU_AWAY].low_slot   = 6;
      queues[QU_AWAY].high_slot  = 12;
      queues[QU_AWAY].q_flags   |= QU_OPEN;

      queues[QU_HOME_OBS].free_slots = 6;  /* Off by default, inlbot gives 2 */
      queues[QU_HOME_OBS].max_slots  = 6;
      queues[QU_HOME_OBS].tournmask  = KLI;
      queues[QU_HOME_OBS].low_slot   = 12;
      queues[QU_HOME_OBS].high_slot  = 20;
      queues[QU_HOME_OBS].q_flags   |= QU_OPEN;

      queues[QU_AWAY_OBS].free_slots = 6;  /* Off by default, inlbot gives 2 */
      queues[QU_AWAY_OBS].max_slots  = 6;
      queues[QU_AWAY_OBS].tournmask  = ORI;
      queues[QU_AWAY_OBS].low_slot   = 12;
      queues[QU_AWAY_OBS].high_slot  = 20;
      queues[QU_AWAY_OBS].q_flags   |= QU_OPEN;

      if (inhl > 1){ /* Allow observers on either team */
	queues[QU_HOME_OBS].tournmask  = KLI|ORI;
	queues[QU_AWAY_OBS].tournmask  = KLI|ORI;
      }

      if (inhl > 2){ /* Allow players on either team */
	queues[QU_HOME].tournmask  = KLI|ORI;
	queues[QU_AWAY].tournmask  = KLI|ORI;
      }
      
    }
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
    me->p_war = 0;
    me->p_team = 0;	/* indep */
    anncer->p_hostile = 0;
    anncer->p_swar = 0;
    anncer->p_team = 0;	/* indep */
    anncer->p_ship.s_repair = 30000;

/*    myship->s_type = STARBASE;*/
    myship->s_type = SCOUT;
    myship->s_mass = 200;
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
    /* Rink definitions moved to puckdefs.h - MK 9-20-93 */

    /* 31 = Cas, 33 = Spi,	            Ori goal line
       35 = Pol, 36 = Arc, 39 = Ant         end of Ori goal */

planets[31].pl_owner = ORI; planets[31].pl_x = G_RGT; planets[31].pl_y = ORI_G;
planets[33].pl_owner = ORI; planets[33].pl_x = G_LFT; planets[33].pl_y = ORI_G;
planets[35].pl_owner = ORI; planets[35].pl_x = G_LFT; planets[35].pl_y = ORI_E;
planets[36].pl_owner = ORI; planets[36].pl_x = G_MID; planets[36].pl_y = ORI_E;
planets[39].pl_owner = ORI; planets[39].pl_x = G_RGT; planets[39].pl_y = ORI_E;

/* 26 = Sco, 25 = Lyr,                Kli goal
   22 = And, 28 = Sco, 24 = Pol,      end of Kli goal */

planets[26].pl_owner = KLI; planets[26].pl_x = G_LFT; planets[26].pl_y = KLI_G;
planets[25].pl_owner = KLI; planets[25].pl_x = G_RGT; planets[25].pl_y = KLI_G;
planets[22].pl_owner = KLI; planets[22].pl_x = G_LFT; planets[22].pl_y = KLI_E;
planets[28].pl_owner = KLI; planets[28].pl_x = G_MID; planets[28].pl_y = KLI_E;
planets[24].pl_owner = KLI; planets[24].pl_x = G_RGT; planets[24].pl_y = KLI_E;

    /* 34 = Pro, 21 = Pliedes V, 32 = El, 27 = Mir      red line */

planets[34].pl_owner = IND; planets[34].pl_x = RED_1; planets[34].pl_y = R_MID;
planets[21].pl_owner = IND; planets[21].pl_x = RED_2; planets[21].pl_y = R_MID;
planets[32].pl_owner = IND; planets[32].pl_x = RED_3; planets[32].pl_y = R_MID;
planets[27].pl_owner = IND; planets[27].pl_x = RED_4; planets[27].pl_y = R_MID;

    /* 23 = Lal, 20 = Kli, 29 = Cas     Kli blue line */
    /* 37 = Urs, 30 = Ori, 38 = Her     Ori blue line */

planets[23].pl_owner = IND; planets[23].pl_x = RED_1; planets[23].pl_y = KLI_B;
planets[20].pl_owner = KLI; planets[20].pl_x = R_MID; planets[20].pl_y = KLI_B;
planets[29].pl_owner = IND; planets[29].pl_x = RED_4; planets[29].pl_y = KLI_B;

planets[37].pl_owner = IND; planets[37].pl_x = RED_1; planets[37].pl_y = ORI_B;
planets[30].pl_owner = ORI; planets[30].pl_x = R_MID; planets[30].pl_y = ORI_B;
planets[38].pl_owner = IND; planets[38].pl_x = RED_4; planets[38].pl_y = ORI_B;

/*  This was the old team lockout code, which is redundant with the
    new waitq format */
/*    planets[0].pl_couptime = 999999; */  /* no Feds */
/*    planets[10].pl_couptime = 999999;*/  /* no Roms */

    for (i=0; i<10; i++) {
	planets[i].pl_owner = IND;
	planets[i].pl_x = RINK_LEFT;
	planets[i].pl_y = RINK_TOP+ i * TENTH + TENTH/2;
	planets[i+10].pl_owner = IND;
	planets[i+10].pl_x = RINK_RIGHT;
	planets[i+10].pl_y = RINK_TOP+ i * TENTH + TENTH/2;
	planets[i+00].pl_flags |= (PLFUEL | PLREPAIR);
	planets[i+10].pl_flags |= (PLFUEL | PLREPAIR);
	planets[i+20].pl_flags |= (PLFUEL | PLREPAIR);
	planets[i+30].pl_flags |= (PLFUEL | PLREPAIR);
	planets[i+00].pl_flags &= ~PLAGRI;  /* ***BAV*** */
	planets[i+10].pl_flags &= ~PLAGRI;
	planets[i+20].pl_flags &= ~PLAGRI;
	planets[i+30].pl_flags &= ~PLAGRI;

	planets[i+00].pl_info |= ALLTEAM;
	planets[i+10].pl_info |= ALLTEAM;
	planets[i+20].pl_info |= ALLTEAM;
	planets[i+30].pl_info |= ALLTEAM;
    }
}

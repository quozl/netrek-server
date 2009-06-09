/* robotII.c
 */
#include "copyright.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "alarm.h"
#include "roboshar.h"
#include "util.h"
#include "slotmaint.h"

extern int redrawall;		/* maint: missing "extern" 6/22/92 TC */
extern int lastm;		/* maint: missing "extern" 6/22/92 TC */

extern char roboname[15];  /* So it can be defined dynamically */

void config();

/* lots of neat flags */
int hostile;
int debug;
int level;
int fleet;
int sticky;
int berserk;
int practice;
int nofuel;
int cloaker;			/* plocking trainings robot 10/15/94 [007] */
int polymorphic;		/* match opponent's ship class 8/15/91 TC */

int target;			/* Terminator's target 7/27/91 TC */
int phrange;			/* phaser range 7/31/91 TC */
int trrange;			/* tractor range 8/2/91 TC */

int quiet = 0;			/* Don't bug users with silly messages */

/* velocities 8/9/91 TC */

int dogslow;			/* dodge speed (hard turn) */
int dogfast;			/* dodge speed (soft turn) */
int runslow;			/* run speed (flat etemp) */
int runfast;			/* run speed (gain etemp) */
int closeslow;			/* approach speed (hard turn) */
int closefast;			/* approach speed (soft turn) */

/* char *rnames[4] = { "M5", "Colossus", "Guardian", "HAL"};*/
char *rnames[6] = { "M5", "Colossus", "Guardian", "HAL", "HunterKiller",
		    "TERMINATOR"};

int main(argc, argv)
int argc;
char **argv;
{
    register int i;
    void rmove();
    int team = -1;
    int bteam = FED;
    int pno;
    int class;			    /* ship class 8/9/91 TC */
    
    char pseudo[PSEUDOSIZE]; /* was a global 9/30/94/ MK */
    
    class = SGALAXY;	
    
    getpath();  		/* added 11/6/92 DRG */
    target = -1;		/* no target 7/27/91 TC */
    for( ; argc>1 && argv[1][0]=='-'; argc--,argv++) {
	switch(argv[1][1]) {
	case 'F':
	    nofuel++;
	    break;
        case 'C':
            cloaker++;
            break;
	case 'P':
	    polymorphic++;
	    break;
	case 'f':
	    fleet++;
	    break;
	case 's':
	    sticky++;
	    break;
	case 'd':
	    debug++;
	    break;
	case 'h':
	    hostile++;
	    break;
	case 'p':
	    practice++;
	    class = BATTLESHIP; /* Set Hoser's default ship to BATTLESHIP */
#ifdef RANDOM_PRACTICE_ROBOT
	    {
	    	int pickship;
		int types;
		
		types = 4;
#ifdef SGALAXY
		types = 6;
#endif
		srandom(getpid() * time((time_t *) 0)); /* Seed random gen. */
		pickship = random() % types; /* Get random ship type */
		switch (pickship) {
		   case 0:     class = SCOUT;	     break;
		   case 1:     class = DESTROYER;    break;
		   case 2:     class = CRUISER;	     break;
		   case 3:     class = BATTLESHIP;   break;
		   case 4:     class = ASSAULT;	     break;
		   case 5:     class = SCOUT;        break;  /* not an SB */
#ifdef SGALAXY
		   case 6:     class = SGALAXY;	     break;
#endif
		   default:    class = SCOUT;        break;  /* unreachable */
		}
	    }
#endif
	    break;
	case 'b':
	    berserk++;
	    break;
	case 'q':
    	    quiet = 1;
	    break;
	case 'l':
	    if (argv[1][2] != '\0')
		level = atoi(&argv[1][2]);
	    else
		level = 100;
	    break;
	case 'c':		/* ship class */
	    switch (argv[1][2]) {
	    case 'a': class = ASSAULT; break;
	    case 'b': class = BATTLESHIP; break;
	    case 'c': class = CRUISER; break;
	    case 'd': class = DESTROYER; break;
	    case 's': class = SCOUT; break;
	    case 'g': class = SGALAXY; break;
	    case 'o': class = STARBASE; break; /* not fully implemented */
	    default:
		ERROR(2,("Unknown ship class, must be one of [abcdsoA].\n"));
		exit(1);
		break;
	    }
		break;
	case 'T':		/* team */
	    switch (argv[1][2]) {
	    case 'f':
		team = 0;
		bteam = FED;
		break;
	    case 'r':
		team = 1;
		bteam = ROM;
		break;
	    case 'k':
		team = 2;
		bteam = KLI;
		break;
	    case 'o':
		team = 3;
		bteam = ORI;
		break;
	    case 'n':	/* change 5/10/91 TC neutral */
	    case 'i':
		team = 4;
		bteam = FED|ROM|KLI|ORI; /* don't like anybody */
		break;
	    case 't': {
		char c;
		c = argv[1][3];
		
		team = 5;
		bteam = FED|ROM|KLI|ORI; /* don't like anybody */
		target = -1;
		if (c == '\0') {
		    ERROR(2,("Must specify target.  e.g. -Tt3.\n"));
		    exit(1);
		}
		if ((c >= '0') && (c <= '9'))
		    target = c - '0';
		else if ((c >= 'a') && (c <= 'j'))
		    target = c - 'a' + 10;
		else {
		    ERROR(2,("Must specify target.  e.g. -Tt3.\n"));
		    exit(1);
		}
	    }			/* end case 't' */
		break;
	    default:
		ERROR(2,("Unknown team type.  Usage -Tx where x is [frko]\n"));
		exit(1);
	    }			/* end switch argv */
		break;
	default:
	    ERROR(3,( "Unknown option '%c' (must be one of: bcdfpsFPT)\n", argv[1][1]));
	    exit(1);
	}			/* end switch argv[1][1] */
	
	
    }				/* end for */
    srandom(getpid() * time((time_t *) 0));
    if (team < 0 || team >= 6) { /* change 7/27/91 TC was 4 now 6 */
	if (debug)
	    ERROR(1,( "Choosing random team.\n"));
	team = random() % 4;
    }
    openmem(1);
    readsysdefaults();
    if ( (pno = pickslot(QU_ROBOT)) < 0) exit(0);
    me = &players[pno];
    myship = &me->p_ship;
    mystats = &me->p_stats;
    lastm = mctl->mc_current;

    /* At this point we have memory set up.  If we aren't a fleet, we don't
       want to replace any other robots on this team, so we'll check the
       other players and get out if there are any on our team.
    */

    if (practice) {
	strcpy(pseudo, "Hoser");
    } else {
	strcpy(pseudo, rnames[team]);
    }

    robonameset(me);  /* set the robot@nowhere fields */

    if (target >= 0)		/* hack 7/27/91 TC */
	enter(team, target, pno, class, pseudo); /* was BATTLESHIP 8/9/91 TC */
    else
	enter(team, 0, pno, class, pseudo); /* was BATTLESHIP 8/9/91 TC */

    me->p_pos = -1;			/* So robot stats don't get saved */
    me->p_flags |= PFROBOT;			/* Mark as a robot */
    if ((berserk) || (team == 4)) /* indeps are hostile */
	me->p_hostile = (FED|ROM|KLI|ORI);	/* unless they are berserk */
    else if (team == 5)
	me->p_hostile = 0;	/* Termies declare war later */
    else if (practice)
	me->p_hostile = bteam;			/* or practice */
    else
	me->p_hostile = 0;			/* robots are peaceful */
    me->p_war = me->p_hostile;

    /* Set up the roboname variable */
    sprintf(roboname, "%c%c", teamlet[me->p_team], shipnos[me->p_no]);

    if (practice)
	me->p_flags |= PFPRACTR;		/* Mark as a practice robot */
    alarm_init();

    config();

    me->p_process = getpid();
    p_ups_set(me, practice ? 1 : 2);

    if (team == 4) { 
	int count = 0;
	for (i = 0; i < MAXPLAYER; i++) {
	    if (players[i].p_status == PALIVE)
		count++;
	}
	if (((count <= 1) && (!fleet)) ||
	    ((count < 1) && (fleet))) {	/* if one or less players, don't show */
	    if (debug)
		ERROR(1,( "No one to hose.\n"));
	    freeslot(me);
	    exit(1);
	}
    }
    
    
    if (!fleet) { 
	for (i = 0; i < MAXPLAYER; i++) {
	    if ((players[i].p_status == PALIVE) && (players[i].p_team == bteam)) {
		if (debug)
		    ERROR(1,( "Galaxy already defended\n"));
		freeslot(me);
		exit(1);
	    }
	}
    }

    me->p_status = PALIVE;		/* Put robot in game */
    if (cloaker) cloak_on();
    while ((status -> gameup & GU_GAMEOK) && (me -> p_status != PFREE)) {
        alarm_wait_for();
        rmove();
    }
    exit(1);
}


void config()
{
    /* calc class-specific stuff */

    phrange = PHASEDIST * me->p_ship.s_phaserdamage / 100;
    trrange = TRACTDIST * me->p_ship.s_tractrng;

    switch (myship->s_type) {
      case SCOUT:
	dogslow = 5;
	dogfast = 7;
	runslow = 8;
	runfast = 9;
	closeslow = 5;
	closefast = 7;
	break;
      case DESTROYER:
	dogslow = 4;
	dogfast = 6;
	runslow = 6;
	runfast = 8;
	closeslow = 4;
	closefast = 6;
	break;
      case CRUISER:
	dogslow = 4;
	dogfast = 6;
	runslow = 6;
	runfast = 7;
	closeslow = 4;
	closefast = 6;
	break;
      case SGALAXY:
	dogslow = 4;
	dogfast = 6;
	runslow = 6;
	runfast = 7;
	closeslow = 4;
	closefast = 6;
	break;
      case BATTLESHIP:
	dogslow = 3;
	dogfast = 5;
	runslow = 5;
	runfast = 6;
	closeslow = 3;
	closefast = 4;
	break;
      case ASSAULT:
	dogslow = 3;
	dogfast = 5;
	runslow = 6;
	runfast = 7;
	closeslow = 3;
	closefast = 4;
	break;
      case STARBASE:
	dogslow = 2;
	dogfast = 2;
	runslow = 2;
	runfast = 2;
	closeslow = 2;
	closefast = 2;
	break;
    }

    if (debug) ERROR(1,("My phaser range: %d.\n", phrange));
    if (debug) ERROR(1,("My tractor range: %d.\n", trrange));

    if (nofuel) {
	myship->s_phasercost = 0;
	myship->s_torpcost = 0;
	myship->s_cloakcost = 0;
    }
    if (target >= 0) {		/* 7/27/91 TC */
	myship->s_maxspeed = 10; /* so you can't run */
	myship->s_warpcost = 1;
	myship->s_egncoolrate = 100;
    }
    if (cloaker) {
	myship->s_repair = 1000;	/* 10x faster repair rate */
    }
}

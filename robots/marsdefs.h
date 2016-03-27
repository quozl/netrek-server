/* marsdefs.h

   This file contains many of the defines needed for Netrek Hockey
   and not for anything else.

   Cough... well... now it contains the Mars defines as well. jn
*/
#ifndef _h_marsdefs
#define _h_marsdefs

void do_war(void);
void mars_rules(void);
void init_mars(void);

#include "defs.h"

/* Mars specific additions */
#define TOURNMAP    "tourn.map"
#define STARTPLANET 10
#define MAXARENAS      9
#define LEFT1 10000
#define RIGHT1 30000
#define LEFT2 70000
#define RIGHT2 90000
#define VERT1 10000
#define VERT2 30000
#define VERT3 50000
#define VERT4 70000
#define VERT5 90000

/* 
  mutual means we killed each other with weapons or the second
  explosion player was killed by explosion
*/
#define MUTUALVALUE 0.7

/* ogg means my explosion killed him */
#define OGGVALUE 0.5

#define badArenaNo(a) ((a) < 0 || (a) > (MAXARENAS-1))

#define checkBadPlayerNo(a,x)    { if (badPlayerNo(a)) { \
	ERROR(2,("Bad player number %d;  %s  Exitting.\n",a,x)); \
	cleanup(); \
      } }
#define checkBadArenaNo(a,x)    { if (badArenaNo(a)) { \
	ERROR(2,("Bad arena number %d;  %s  Exitting.\n",a,x)); \
	cleanup(); \
      } }


/* Game Control Parameters - all of these apply to Mars */
#define ALLOWED_SHIPS    5	/* determines which ships the player is allowed
				  to play in the "prefered ship" round.
				  5 = sc,dd,ca,bb,as
				  6 = above + sb
				  7 = above + att
				*/
#define SCHEDULER		/* better scheduler for players, more cpu cost*/

/*#define ALLOWQUIT */		/* allow players to enter sitout mode anytime*/
#define WALL_BOUNCE		/* bouncy walls or sticky walls?*/
#define NEWGAME_RESET		/* define to have newgame reset stats*/
/*#define NO_BRUTALITY */	/* waiting arena has peace*/

/* Some "computer speed" definitions */
#define HOWOFTEN 1                       /*Robot moves every HOWOFTEN cycles*/
#define PERSEC (1000000/UPDATE/HOWOFTEN) /* # of robo calls per second*/

#define TELEPORT_DELAY 1        /*Time between goal score and teleport home*/

/* Rink Definitions */
#define RINK_TOP 0
#define RINK_BOTTOM GWIDTH
#define TENTH ((RINK_BOTTOM - RINK_TOP)/10)
#define R_MID ((RINK_BOTTOM - RINK_TOP)/2) /* center (red) line */
#define RINK_LENGTH (RINK_BOTTOM - RINK_TOP)
 
#define RINK_WIDTH (GWIDTH*2/3)
#define G_MID (GWIDTH/2)        /* center of goal */
#define RINK_LEFT (G_MID-(RINK_WIDTH/2))
#define RINK_RIGHT (G_MID+(RINK_WIDTH/2))
 
#define G_LFT (R_MID-TENTH)     /* left edge of goal */
#define G_RGT (R_MID+TENTH)     /* right edge of goal */
#define RED_1 (RINK_LEFT + (1*RINK_WIDTH/5))
#define RED_2 (RINK_LEFT + (2*RINK_WIDTH/5))
#define RED_3 (RINK_LEFT + (3*RINK_WIDTH/5))
#define RED_4 (RINK_LEFT + (4*RINK_WIDTH/5))
 
#define ORI_G (RINK_BOTTOM - /*2* */TENTH) /* Ori goal line */
#define ORI_E (RINK_BOTTOM -   TENTH/2) /* end of Ori goal */
#define ORI_B (RINK_BOTTOM - (RINK_LENGTH/3)) /* Ori blue line */
#define KLI_G (RINK_TOP    +/* 2* */ TENTH) /* Kli goal line */
#define KLI_E (RINK_TOP    +   TENTH/2) /* end of Kli goal */
#define KLI_B (RINK_TOP    + (RINK_LENGTH/3)) /* Kli blue line */


/* Some global variables */
extern char *roboname;

#endif /* _h_marsdefs */


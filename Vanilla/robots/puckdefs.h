/* Change Log
    
   Created:  9-20-93 by Michael Kantner
   Added:    9-24-93 WALL_BOUNCE and GOAL_CLEAR
   Added:    9-27-93 NEWGAME_RESET
   Added:    9-29-93 NO_BRUTALITY
   Added:   10-06-93 PV_* Generic Voting Stuff (moved to puckmove.c)
   Added:   10-06-93 ALLOW_EJECT
   Added:   10-07-93 SHORT_GAME
   Added:   10-12-93 ANNOUNCER
   Added:   10-29-93 OFFSIDE

   Change:   1-??-94 Modified for server version 2.5.  Commands and
                     voting moved to separate file.
   Change:   6-20-94 Moved SHORT_GAME to a votable feature.
   Change:   6-20-94 Added NUMSKATERS definition.
   Added:    6-20-94 Re-added ALLOW_EJECT, may conflict with server

   Added:    9-20-94 Added HOWOFTEN, and changed PERSEC.
   Added:    5-30-95 SHOT_DESC for added shot descriptions.
   Added:    5-14-97 LIGHT_PLANETS to have planets indicate score
*/

/* puckdefs.h

   This file contains many of the defines needed for Netrek Hockey
   and not for anything else.
*/


/* Game Control Parameters */
/*#define SOCCER_SHOOT */	/*define for soccer-style shooting */
/*#define HAVE_GOALIE */	/*define to have goalie ships */
/*#define GOAL_CLEAR */		/*define to keep players outside the goals*/
/*#define NO_BRUTALITY */	/*define to make peaceful when goal scores*/
/*#define OFFSIDE  */		/*define to have hockey offside rules*/

#undef NO_BRUTALITY               /*Must also be undefined, if needed */
#define ANNOUNCER                 /*define for more loquacious commentary*/
#define NEWGAME_RESET             /*define to have newgame reset stats*/
#define WALL_BOUNCE               /*define to keep players in the rink*/
#define SHOT_DESC		  /*define to have on goal descriptions.  */
#define LIGHT_PLANETS             /*define to have planets show the score*/

#define NUMSKATERS	12

                       /* With SITOUT_HURTS defined, sitout causes
                          ships to be severely damaged (in order to prevent
                          abuse of sitout to gain position). If undefined,
                          sitout does not damage, but also does not
                          change a ship's y-position (allowing ships to
                          return to the rink quickly if a sitout wasn't
                          really needed, etc, which happens far more often
                          than abuse). Default was defined  */
#define SITOUT_HURTS
 


/* Some "computer speed" definitions */
#define HOWOFTEN 1                       /*Robot moves every HOWOFTEN cycles*/
#define PERSEC (1000000/UPDATE/HOWOFTEN) /* # of robo calls per second*/

#define TELEPORT_DELAY 5        /*Time between goal score and teleport home*/


/* Rink Definitions */
#define RINK_TOP 0
#define RINK_BOTTOM GWIDTH
#define TENTH ((RINK_BOTTOM - RINK_TOP)/10)
#define R_MID ((RINK_BOTTOM - RINK_TOP)/2) /* center (red) line */
#define RINK_LENGTH (RINK_BOTTOM - RINK_TOP)

#define RINK_WIDTH (GWIDTH*2/3)
#define G_MID (GWIDTH/2)	/* center of goal */
#define RINK_LEFT (G_MID-(RINK_WIDTH/2))
#define RINK_RIGHT (G_MID+(RINK_WIDTH/2))

#define G_LFT (R_MID-TENTH)	/* left edge of goal */
#define G_RGT (R_MID+TENTH)	/* right edge of goal */
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

                       /* FO_BIAS adds in code for biasing Faceoff against
                          the scoring team, or a team ahead by more than 5
                          points.  Leaving this undefined is the default 
                          behavior of leaving puck in the center without
                          FACEOFF_HELP code */
/* #undef FO_BIAS */
                       /* FACEOFF_HELP should be defined, either as 0
                          to leave the default behaviour, or > 0 as the
                          offset to apply to the puck's droppoint at faceoff
                          to assist the losing team. */
#define FACEOFF_HELP (TENTH*2/3)


/* Some global puck variable definitions */
extern struct player *anncer;
extern char *roboname;

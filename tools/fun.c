/* Server Fun Tools		by Kurt Siegl <siegl@risc.uni-linz.ac.at>
 *
 * Top Gun + Moving Planets + Send Funny Messages
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/file.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <pwd.h>
#include <ctype.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

void action(int ignored);
void usage(char *string);
void funnymessage(void);
void pmove(void);
void start_topgun(void);
void rules(void);
void topgunships(void);
void stop_topgun(void);


char buf[MSG_LEN + 20];

#define COS(x) ((x) >= 0.0 ? Cosine[(int)(x)] : Cosine[(int)(-(x))])
#define SIN(x) ((x) >= 0.0 ? Sine[(int)(x)] : -Sine[(int)(-(x))])

#define SYSDEF_SAVE_FILENAME LOCALSTATEDIR"/.sysdef.topgunrestore"
#define SAVE_SYSDEF_CMD "cp "LOCALSTATEDIR"/.sysdef "LOCALSTATEDIR"/.sysdef.topgunrestore"
#define TOPGUN_SYSDEF_CMD1 "cp "LOCALSTATEDIR"/.sysdef.topgun "LOCALSTATEDIR"/.sysdef.temp"
#define TOPGUN_SYSDEF_CMD2 "mv "LOCALSTATEDIR"/.sysdef.temp "LOCALSTATEDIR"/.sysdef"
#define RESTORE_SYSDEF_CMD "mv "LOCALSTATEDIR"/.sysdef.topgunrestore "LOCALSTATEDIR"/.sysdef"

#define PUPDATE 0
#define PUPDATE_SEC 1

int rotall = 0, rotcore = 0;
int mesdelay = -1, rotdelay = 5; 
int topguntime = 600, topgundelay = -1; 
int rtimer= -1, mtimer= -1, ttimer= -1;

int pl_home[4];
int pl_core[4][4];
int pl_dist1[4][4];
int pl_dist[MAXPLANETS];
double increment = 0.01;
double incrementrecip = 100.0;
float *Cosine, *Sine;

double dpre;
double fpre;
double pi = 3.1415926;

int main(int argc, char **argv)
{
    double dx, dy;
    int i,j;

    int pre;

    static struct itimerval udt;

    openmem(0);

    for (i=1; i<argc; i++) {
	if (argv[i][0]=='-') {
	    switch(argv[i][1]) {
	      case 't': 
		topgun=1;
		if (argv[i+1] && (argv[i+1][0] != '-')) 
		   topguntime = atoi(argv[++i]);
		if (argv[i+1] && (argv[i+1][0] != '-')) 
		   topgundelay = atoi(argv[++i]);
		break;
	      case 'r':
		rotcore = 1;
		break;
	      case 'R':
		rotall = 1;
		break;
	      case 'd':
		if (argv[i+1] && (argv[i+1][0] != '-')) 
		   rotdelay = atoi(argv[++i]);
		break;
	      case 'i':
		if (argv[i+1] && (argv[i+1][0] != '-')) 
		   sscanf(argv[++i], "%lf", &increment);
		   incrementrecip=1/increment;
		/* printf("Increment: %f.\n", increment); */
		break;
	      case 'm':
		if (argv[i+1] && (argv[i+1][0] != '-')) 
		   mesdelay = atoi(argv[++i]);
		else mesdelay = 60;
		break;
	    default:
		usage(argv[0]);
		exit(1);
		break;
	    }
	}
    }

    pre = 3.5/increment;
    dpre = (double) pre;

    Cosine = (float*) calloc(sizeof(float), pre);
    Sine = (float*) calloc(sizeof(float), pre);
    for (i = 0; i < pre; i++) {
	Cosine[i] = cos((double)i*increment);
	Sine[i] = sin((double)i*increment);
    }

    pl_home[0] = 0; pl_core[0][0] = 5; pl_core[0][1] = 7; pl_core[0][2] = 8; pl_core[0][3] = 9; 
    pl_home[1] = 10; pl_core[1][0] = 12; pl_core[1][1] = 15; pl_core[1][2] = 16; pl_core[1][3] = 19;
    pl_home[2] = 20; pl_core[2][0] = 24; pl_core[2][1] = 26; pl_core[2][2] = 29; pl_core[2][3] = 25; 
    pl_home[3] = 30; pl_core[3][0] = 34; pl_core[3][1] = 37; pl_core[3][2] = 38; pl_core[3][3] = 39; 

    for (i = 0; i < 40; i++) {
	dx = (double) (planets[i].pl_x - GWIDTH/2);
	dy = (double) (GWIDTH/2 - planets[i].pl_y);
	pl_dist[i] = sqrt(dx * dx + dy * dy);
    }
    for (j = 0; j < 4; j++) 
      for (i = 0; i < 4; i++) {
        dx = (double) (planets[pl_core[j][i]].pl_x - planets[pl_home[j]].pl_x);
        dy = (double) (planets[pl_home[j]].pl_y - planets[pl_core[j][i]].pl_y);
        pl_dist1[j][i] = sqrt(dx * dx + dy * dy);
	}

    if (topgun) { topgun=0; ttimer=5; } 	/* start topgun in 5 sec */
    rtimer = rotdelay;
    mtimer = mesdelay;

    signal(SIGALRM, action);
    udt.it_interval.tv_sec = PUPDATE_SEC;
    udt.it_interval.tv_usec = PUPDATE;
    udt.it_value.tv_sec = PUPDATE_SEC;
    udt.it_value.tv_usec = PUPDATE;
    (void) setitimer(ITIMER_REAL, &udt, (struct itimerval *) 0);

    for (;;)
      pause();
}

void action(int ignored)
{
  signal(SIGALRM,action);
 
  if (rotall || rotcore)
     if (!(rtimer--)) {
	rtimer=rotdelay;
	pmove();
     }
  
  if (!(mtimer--)) {
    mtimer=mesdelay;
    funnymessage();
  }

  if (topgun) {
      if (!(ttimer--)) {
        stop_topgun();
	topgun = 0;
	ttimer=topgundelay;
      }
     topgunships();
     if ((ttimer > 0) && ((ttimer % 120) == 0))  {
	sprintf(buf, ">                -= T O P    G U N =-      (%d min. left) ", (ttimer/60));
	amessage(buf , 0, MALL);
	rules();
      }    
   } else
     if (!(ttimer--)) {
        start_topgun();
	topgun = 1;
	ttimer=topguntime;
     }

}

void usage(char *string)
{
    printf("Usage: %s [-t n1 n2] [-r] [-R] [-i n] [-m n]\n", string);
    printf("  -t Topgun n1 seconds every n2 seconds\n");
    printf("  -r Rotate core \n");
    printf("  -R Rotate galaxy \n");
    printf("  -d delay in seconds between each frame \n");
    printf("  -i increment for rotation\n");
    printf("  -m seconds interval for funny messages\n");
}

void funnymessage(void)
{
  int rnd;

    switch (random()%10) {
	case 0: amessage("GOD->ALL Doosh the Twinks",0,MALL);
		break;
	case 1: rnd=random()%(MAXPLAYER-TESTERS);
		if (players[rnd].p_status == PALIVE) {
		  sprintf(buf,"GOD->ALL %s you really suck!",players[rnd].p_name);
		  amessage(buf,0,MALL);
		}
		break;
	case 2: rnd=random()%(MAXPLAYER-TESTERS);
		if (players[rnd].p_status == PALIVE) {
		  sprintf(buf,"GOD->ALL %s don't scum play for the team",players[rnd].p_name);
		  amessage(buf,0,MALL);
		}
		break;
	case 3: rnd=random()%(MAXPLAYER-TESTERS);
		if ((players[rnd].p_status == PALIVE) && (players[rnd].p_armies == 0)
		    && (players[rnd].p_ship.s_type != STARBASE)) {
	          players[rnd].p_ship.s_type = STARBASE;
        	  players[rnd].p_whydead=KPROVIDENCE;
        	  players[rnd].p_explode=2*SBEXPVIEWS;
        	  players[rnd].p_status=PEXPLODE;
        	  players[rnd].p_whodead=0;
		  sprintf(buf,"GOD->ALL %s (%2s) exploded because clueless play",
			players[rnd].p_name,players[rnd].p_mapchars);
		  amessage(buf,0,MALL | MKILL);
		}
		break;
	case 4: amessage("GOD->ALL   ~|~ /\\ | | /~ |_| |\\ /\\ |   | |\\ |",0,MALL); 
		amessage("GOD->ALL    |  \\/ |_| \\_ | | |/ \\/  \\^/  | \\|",0,MALL);
		break;
	default: amessage("GOD->ALL It's fun time !!!!!!!!!",0,MALL);
		break;
	}
}

void pmove(void) 
{
  int i,j;
  double dir, dx,dy ;

    if (rotcore) 
      for (j =0; j < 4; j++)
	for (i =0; i < 4; i++) {
	  dir =(atan2((double) (planets[pl_core[j][i]].pl_y - planets[pl_home[j]].pl_y),
                       (double) (planets[pl_core[j][i]].pl_x - planets[pl_home[j]].pl_x))
                 );
	  if (dir > pi) dir = dir - 2.0*pi;
	  if (dir >= 0.0)
	     dir = (dir*incrementrecip+1.5);
	  else
	     dir = (dir*incrementrecip+0.5);
          planets[pl_core[j][i]].pl_x = planets[pl_home[j]].pl_x + pl_dist1[j][i] * COS(dir);
          planets[pl_core[j][i]].pl_y = planets[pl_home[j]].pl_y + pl_dist1[j][i] * SIN(dir);
	  dx = (double) (planets[pl_core[j][i]].pl_x - GWIDTH/2);
	  dy = (double) (GWIDTH/2 - planets[pl_core[j][i]].pl_y);
	  pl_dist[i] = sqrt(dx * dx + dy * dy);
          planets[pl_core[j][i]].pl_flags |= PLREDRAW;
        }

    if (rotall) 
      for (i = 0; i < MAXPLANETS; i++) {
	dir = atan2((double) (planets[i].pl_y - GWIDTH/2),
		    (double) (planets[i].pl_x - GWIDTH/2));
/*	printf("Atan2 Dir is %f (%d,%d).\n", dir, planets[i].pl_x,
	       planets[i].pl_y);*/
	if (dir > pi) dir = dir - 2.0*pi;
/*	printf("dir = %f, dir*100 = %f, rint() = %f. %f = %d.\n", dir, dir*100.0, rint(dir*100.0), rint(dir*100.0+1.5), (int) (rint(dir*100.0) + 1.0));*/
	if (dir >= 0.0)
	    dir = (dir*incrementrecip+1.5);
	else
	    dir = (dir*incrementrecip+0.5);

	planets[i].pl_x = GWIDTH/2 + (int) (pl_dist[i] * COS(dir));
	planets[i].pl_y = GWIDTH/2 + (int) (pl_dist[i] * SIN(dir));

	planets[i].pl_flags |= PLREDRAW;
      }
}

void start_topgun(void)
{
    FILE *fp;

    if ((fp = fopen(SYSDEF_SAVE_FILENAME, "r")) == NULL) { /* if we haven't save
d */
        system(SAVE_SYSDEF_CMD); /* save existing .sysdef for shutdown */
        system(TOPGUN_SYSDEF_CMD1); /* make a copy of .sysdef.topgun */
        system(TOPGUN_SYSDEF_CMD2); /* mv it as .sysdef (to be atomic) */
    }
    else /* don't overwrite saved sysdef (how'd it get there?) */
        fclose(fp);

    amessage("GOD->ALL",0, MALL);
    sprintf(buf,"GOD->ALL  Top Gun rules are in effect for %d minutes.",
            topguntime/60);
    amessage(buf, 0, MALL);
    rules();
}


void topgunships(void)
{
  int i;

        for (i=0; i<MAXPLAYER; i++) {
            if (players[i].p_status == PFREE)
                continue;
            players[i].p_ship.s_torpdamage = 130;
            players[i].p_ship.s_plasmaspeed = 25;
            players[i].p_ship.s_plasmacost = 1500;
            players[i].p_ship.s_torpcost = 100;
            if (players[i].p_ship.s_type == SCOUT) {
                players[i].p_ship.s_plasmadamage = 75;
                players[i].p_ship.s_plasmafuse = 25;
                players[i].p_ship.s_plasmaturns = 1;
            }
            else if (players[i].p_ship.s_type == ASSAULT) {
                players[i].p_ship.s_plasmadamage = 130;
                players[i].p_ship.s_plasmafuse = 30;
                players[i].p_ship.s_plasmaturns = 1;
            }
            /*          players[i].p_ship.s_phaserdamage=200;*/
             switch (players[i].p_ship.s_type) { 
               case CRUISER:
                 players[i].p_ship.s_plasmaturns = 2; /* double turn rate? */
               case BATTLESHIP:
               case ASSAULT:
                 players[i].p_ship.s_plasmaturns = 3; /* triple turn rate? */
             }
        }                       /* end for */
}

void rules(void)
{
    amessage(">  Torp damage  : 130 pts.  Torp cost  :  100 fuel.", 0, MALL);
    amessage(">  Plasma damage: normal    Plasma cost: 1500 fuel.  Plasmas enhanced.", 0, MALL);
    amessage(">  No kills required for plasmas.  SB rebuild time is zero.", 0, MALL);
}

void stop_topgun(void)
{
    int player;

    amessage("GOD->ALL", 0, MALL);
    sprintf(buf,"GOD->ALL  Top Gun Rules are no longer in effect.");
    amessage(buf, 0, MALL);
    amessage("GOD->ALL", 0, MALL);
    system(RESTORE_SYSDEF_CMD);

#ifndef nodef
    sleep(1);	/* Top Gun ships are too powerfull, blow them up */
    for (player=0; player<MAXPLAYER; player++) {
        if (players[player].p_status == PFREE)
            continue;
        players[player].p_explode=5;
        players[player].p_status=3;
        players[player].p_whydead=KPROVIDENCE;
        players[player].p_whodead=0;
    }
#endif
}


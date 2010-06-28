/* setgalaxy.c

   usage:
   setgalaxy l              restore planet locations
   setgalaxy r              standard reset of galaxy
   setgalaxy t              tourney reset of galaxy - equal agris
   setgalaxy d              dogfight-friendly reset galaxy
   setgalaxy f              flatten all planets to 1 army
   setgalaxy F              top out all planets at START_ARMIES
   setgalaxy n <num>:<str>  rename planet <num> to <str>
   setgalaxy C              cool server idea from felix@coop.com 25 Mar 1994
   setgalaxy Z              close up shop for maintenance
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "planet.h"
#include "util.h"

static void CoolServerIdea(void);
static void CloseUpShop(void);
static void doINLResources(void);

#define AGRI_LIMIT	3

static void usage(void)
{
    printf("   usage:\n");
    printf("   setgalaxy l              restore planet locations\n");
    printf("   setgalaxy t              reset of galaxy - equal agris\n");
    printf("   setgalaxy d              reset of galaxy - dogfight-friendly\n");
    printf("   setgalaxy f              flatten all planets to 1 army\n");
    printf("   setgalaxy F (num)        top out all planets at (num) armies\n");
    printf("   setgalaxy n <num>:<str>  rename planet <num> to <str>\n");
    printf("   setgalaxy C              triple planet mayhem\n");
    printf("   setgalaxy Z              close up shop for maintenance\n");
}

int main(int argc, char **argv)
{
    int i;
    int top_armies = 17;

    srandom(getpid());
    openmem(0);
    if (argc == 1) { usage(); return 1; }

    if (argc == 3) {
      if ((*argv[1] == 'F') || (*argv[1] == 'r') || (*argv[1] == 't')) 
      top_armies = atoi(argv[2]);
      else {

	int num;
	char name[NAME_LEN];
	if (sscanf(argv[2], "%d:%[^\n]", &num, name)==2) {
	    if ((num >= 0) && (num <= (MAXPLANETS-1))) {
		printf("Renaming planet #%d to %s.\n", num, name);
		strcpy(planets[num].pl_name, name);
		planets[num].pl_namelen = strlen(name);
		planets[num].pl_flags |= PLREDRAW;
	    } else {
		printf("Planet number must be in range (0-%d).\n", MAXPLANETS-1);
		return 1;
	    }
	    return 0;
	} else {
	  usage(); return 1;
	}
      }
    }
 
    if (*argv[1] == 'C') {
        CoolServerIdea();
        return 0;
    }

    if (*argv[1] == 'Z') {
        CloseUpShop();
        return 0;
    }

    if (*argv[1] == 'l') {
        struct planet *virginal = pl_virgin();
	for (i = 0; i < MAXPLANETS; i++) {
	    planets[i].pl_x = virginal[i].pl_x;
	    planets[i].pl_y = virginal[i].pl_y;
	}
	printf("Restored locations.\n");
	return 0;
    }

    if (*argv[1] == 'f') { /* flatten planets */
	for (i = 0; i < MAXPLANETS; i++) {
	    planets[i].pl_armies = 1;
	}
	printf("All planets set to 1 army.\n");
	return 0;
    }

    if (*argv[1] == 'F') { /* top out planets */
	for (i = 0; i < MAXPLANETS; i++) {
	    planets[i].pl_armies = top_armies;
	}
	printf("All planets set to %d armies.\n",top_armies);
	return 0;
    }

    if (*argv[1] == 't' || *argv[1] == 'r') { /* tourney reset resources, owners */
	memcpy(planets, pl_virgin(), pl_virgin_size());
	for (i = 0; i < MAXPLANETS; i++) {
	    planets[i].pl_armies = top_armies;
	}
	doINLResources();
	/* reset the SB construction and surrender countdown immediately */
	for (i = 0; i <= MAXTEAM; i++) {
	  teams[i].te_turns = 0;
	  teams[i].te_surrender = 0;
	}
	return 0;
    }

    if (*argv[1] == 'd') { /* dogfight-friendly galaxy reset */
	memcpy(planets, pl_virgin(), pl_virgin_size());
	for (i = 0; i < MAXPLANETS; i++) {
	    planets[i].pl_armies = 0;
	    planets[i].pl_owner = NOBODY;
	    planets[i].pl_flags |= PLREPAIR;
	    planets[i].pl_flags |= PLFUEL;
	    planets[i].pl_info = ALLTEAM;
	}
	return 0;
    }

    usage();
    return 1;
}

/* the four close planets to the home planet */
static int core_planets[4][4] =
{{ 7, 9, 5, 8,},
 { 12, 19, 15, 16,},
 { 24, 29, 25, 26,},
 { 34, 39, 38, 37,},
};
/* the outside edge, going around in order */
static int front_planets[4][5] =
{{ 1, 2, 4, 6, 3,},
 { 14, 18, 13, 17, 11,},
 { 22, 28, 23, 21, 27,},
 { 31, 32, 33, 35, 36,},
};

static void doINLResources(void)
{
  int i, j, k, which;
  for (i = 0; i < 4; i++){
    /* one core AGRI */
    planets[core_planets[i][random() % 4]].pl_flags |= PLAGRI;

    /* one front AGRI */
    which = random() % 2;
    if (which){
      planets[front_planets[i][random() % 2]].pl_flags |= PLAGRI;

      /* place one repair on the other front */
      planets[front_planets[i][(random() % 3) + 2]].pl_flags |= PLREPAIR;

      /* place 2 FUEL on the other front */
      for (j = 0; j < 2; j++){
      do {
        k = random() % 3;
      } while (planets[front_planets[i][k + 2]].pl_flags & PLFUEL) ;
      planets[front_planets[i][k + 2]].pl_flags |= PLFUEL;
      }
    } else {
      planets[front_planets[i][(random() % 2) + 3]].pl_flags |= PLAGRI;

      /* place one repair on the other front */
      planets[front_planets[i][random() % 3]].pl_flags |= PLREPAIR;

      /* place 2 FUEL on the other front */
      for (j = 0; j < 2; j++){
      do {
        k = random() % 3;
      } while (planets[front_planets[i][k]].pl_flags & PLFUEL);
      planets[front_planets[i][k]].pl_flags |= PLFUEL;
      }
    }

    /* drop one more repair in the core (home + 1 front + 1 core = 3 Repair)*/
    planets[core_planets[i][random() % 4]].pl_flags |= PLREPAIR;

    /* now we need to put down 2 fuel (home + 2 front + 2 = 5 fuel) */
    for (j = 0; j < 2; j++){
      do {
      k = random() % 4;
      } while (planets[core_planets[i][k]].pl_flags & PLFUEL);
      planets[core_planets[i][k]].pl_flags |= PLFUEL;
    }
  }
}

static void CoolServerIdea(void)
{
    int i;

    for (i=0; i<MAXPLANETS; i++)
    {
      planets[i].pl_flags = 0;
      planets[i].pl_owner = 0;
      planets[i].pl_x = -10000;
      planets[i].pl_y = -10000;
      planets[i].pl_info = 0;
      planets[i].pl_armies = 0;
      strcpy ( planets[i].pl_name, "" );
    }

    planets[20].pl_couptime = 999999; /* no Klingons */
    planets[30].pl_couptime = 999999; /* no Orions */

    /* earth */
    i = 0;
    planets[i].pl_flags |= FED | PLHOME | PLCORE | PLAGRI | PLFUEL | PLREPAIR;
    planets[i].pl_x = 40000;
    planets[i].pl_y = 65000;
    planets[i].pl_armies = 40;
    planets[i].pl_info = FED;
    planets[i].pl_owner = FED;
    strcpy ( planets[i].pl_name, "Earth" );

    /* romulus */
    i = 10;
    planets[i].pl_flags |= ROM | PLHOME | PLCORE | PLAGRI | PLFUEL | PLREPAIR;
    planets[i].pl_x = 40000;
    planets[i].pl_y = 35000;
    planets[i].pl_armies = 40;
    planets[i].pl_info = ROM;
    planets[i].pl_owner = ROM;
    strcpy ( planets[i].pl_name, "Romulus" );

    /* indi */
    i = 18;
    planets[i].pl_flags |= PLFUEL | PLREPAIR;
    planets[i].pl_flags &= ~PLAGRI;
    planets[i].pl_x = 15980;
    planets[i].pl_y = 50000;
    planets[i].pl_armies = 4;
    planets[i].pl_info &= ~ALLTEAM;
    strcpy ( planets[i].pl_name, "Indi" );

    for (i=0; i<MAXPLANETS; i++)
    {
        planets[i].pl_namelen = strlen(planets[i].pl_name);
    }
}

#ifdef notused
static void CloseUp(int i) {
  int m, dx, dy, t = 200;
  dx = (planets[i].pl_x - 50000)/t;
  dy = (planets[i].pl_y - 50000)/t;
  for(m=0; m<t; m++) {
    planets[i].pl_x -= dx;
    planets[i].pl_y -= dy;
    usleep(100000);
  }
  planets[i].pl_x = -10000;
  planets[i].pl_y = -10000;
  planets[i].pl_flags = 0;
  planets[i].pl_owner = 0;
  planets[i].pl_info = 0;
  planets[i].pl_armies = 0;
  strcpy ( planets[i].pl_name, "" );
  planets[i].pl_namelen = strlen(planets[i].pl_name);
  
}

static void CloseUpShop2(void)
{
  int x, y;

  for (y=0; y<5; y++) {
    for (x=0; x<4; x++) {
      CloseUp(front_planets[x][y]);
    }
    sleep(2);
  }
  for (y=0; y<4; y++) {
    for (x=0; x<4; x++) {
      CloseUp(core_planets[x][y]);
    }
    sleep(2);
  }
  CloseUp(0);
  CloseUp(10);
  CloseUp(20);
  CloseUp(30);
}
#endif

static void CloseUpShop() {
  int i, m, dx[MAXPLANETS], dy[MAXPLANETS], t = 600;
  int x1, y1, x2, y2;

  for(i=0; i<MAXPLANETS; i++) {
    dx[i] = (planets[i].pl_x - 50000)/t;
    dy[i] = (planets[i].pl_y - 50000)/t;
  }

  for(m=0; m<t; m++) {
    x1 = y1 = GWIDTH; x2 = y2 = 0;
    for(i=0; i<MAXPLANETS; i++) {
      struct planet *p = &planets[i];
      p->pl_x -= dx[i];
      p->pl_y -= dy[i];
      if (p->pl_x < x1) x1 = p->pl_x;
      if (p->pl_x > x2) x2 = p->pl_x;
      if (p->pl_y < y1) y1 = p->pl_y;
      if (p->pl_y > y2) y2 = p->pl_y;
    }

    /* confine players to shrunken universe */
    if (m < (t-100)) for(i=0; i<MAXPLAYER; i++) {
      p_x_y_box(&players[i], x1, y1, x2, y2);
    }

    if (m == (t-100)) {
      for(i=0; i<MAXPLANETS; i++) {
	planets[i].pl_flags = 0;
	planets[i].pl_owner = 0;
	planets[i].pl_info = ALLTEAM;
	planets[i].pl_armies = 0;
	strcpy ( planets[i].pl_name, "   " );
	planets[i].pl_namelen = strlen(planets[i].pl_name);
      }
    }
    usleep(200000);
  }

  for(i=0; i<MAXPLANETS; i++) {
    planets[i].pl_x = -20000;
    planets[i].pl_y = -20000;
  }

  sleep(2);

  for(i=0; i<MAXPLAYER; i++) {
    players[i].p_team = 0;
    players[i].p_hostile = ALLTEAM;
    players[i].p_swar = ALLTEAM;
    players[i].p_war = ALLTEAM;
    sprintf(players[i].p_mapchars, "%c%c", 
	    teamlet[players[i].p_team], shipnos[i]);
    sprintf(players[i].p_longname, "%s (%s)", 
	    players[i].p_name, players[i].p_mapchars);
  }

  /* don't let them come back? */
  planets[0].pl_couptime = 999999;
  planets[10].pl_couptime = 999999;
  planets[20].pl_couptime = 999999;
  planets[30].pl_couptime = 999999;
}


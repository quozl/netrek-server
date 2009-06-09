#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "planet.h"
#include "util.h"

static void usage(void)
{
  fprintf(stderr, "\
Usage: setplanet dump\n\
       setplanet PLANET COMMAND [COMMAND ...]\n\
\n\
PLANET is either a planet name, prefix of a name, or a number.\n\
COMMAND is one of:\n\
\n\
get           dump planet data for one planet in setplanet format\n\
verbose       send messages to all in game for certain changes\n\
pause         wait one second before proceeding\n\
\n\
FLAG          set a planet flag\n\
noFLAG        clear a planet flag\n\
              flags are (repair fuel agri redraw home coup cheap core)\n\
\n\
owner TEAM    set the owner of the planet to a team\n\
x COORDINATE  change the coordinates of the planet on the galactic\n\
y COORDINATE\n\
restore       restore the coordinates to the standard position\n\
name NAME     change the name of the planet (client may not see it)\n\
armies COUNT  change the number of armies on the planet to COUNT\n\
flatten       set the army count to one\n\
pop           increment the army count\n\
plague        decrement the army count\n\
info MASK     set the information mask (which teams see the planet)\n\
reveal        make planet known to all teams\n\
lose          make planet unknown to non-owning teams\n\
lost          make planet unknown to all teams, including owner\n\
hide          hide the planet by moving it off the galactic\n\
deorbit       break the orbit of any player orbiting the planet\n\
couptime N    set the time allowed before coup\n\
");
}

/* send message to players */
static void say(const char *fmt, ...)
{
  va_list args;
  char buf[80];

  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  amessage(buf, 0, MALL);
  va_end(args);
}

/* display a planet flag */
static void get_flag(char *name, int mask, struct planet *pl)
{
  if (pl->pl_flags & mask) {
    printf(" %s", name);
  } else {
    printf(" no%s", name);
  }
}

/* check for a request to set a planet flag, and do it */
static int set_flag(char *argv, char *name, int mask, struct planet *pl, int verbose)
{
  if (strcmp(argv, name)) return 0;
  if (!(pl->pl_flags & mask)) {
    pl->pl_flags |= mask;
    if (verbose) say("%s made %s", pl->pl_name, name);
    return 1;
  }
  return 0;
}

/* check for a request to clear a planet flag, and do it */
static int clr_flag(char *argv, char *name, int mask, struct planet *pl, int verbose)
{
  if (strcmp(argv, name)) return 0;
  if (pl->pl_flags & mask) {
    pl->pl_flags &= ~mask;
    if (verbose) say("%s made %s", pl->pl_name, name);
    return 1;
  }
  return 0;
}

/* display everything known about a planet in command line format */
void get(char *us, struct planet *pl)
{
  printf("%s %d", us, pl->pl_no);
  get_flag("repair", PLREPAIR, pl);
  get_flag("fuel",   PLFUEL,   pl);
  get_flag("agri",   PLAGRI,   pl);
  get_flag("redraw", PLREDRAW, pl);
  get_flag("home",   PLHOME,   pl);
  get_flag("coup",   PLCOUP,   pl);
  get_flag("cheap",  PLCHEAP,  pl);
  get_flag("core",   PLCORE,   pl);
  printf(" owner %s", team_name(pl->pl_owner));
  printf(" x %d", pl->pl_x);
  printf(" y %d", pl->pl_y);
  printf(" name '%s'", pl->pl_name);
  printf(" armies %d", pl->pl_armies);
  printf(" info %d", pl->pl_info);
  printf(" couptime %d", pl->pl_couptime);
  printf("\n");
}

/* display everything about every planet */
static void dump(char *us) {
  int i;
  for(i=0; i<MAXPLANETS; i++) {
    get(us, &planets[i]);
  }
}

int main(int argc, char **argv)
{
    int i, verbose = 0;
    struct planet *pl;

    if (argc == 1) { usage(); return 1; }
    openmem(0);

    i = 1;

 state_0:
    /* dump - perform a get for each planet */
    if (!strcmp(argv[i], "dump")) {
      dump(argv[0]);
      if (++i == argc) return 0;
      goto state_0;
    }

    /* check for planet identifier */
    if (isalpha((int)argv[i][0])) {
      pl = planet_find(argv[i]);
    } else {
      pl = planet_by_number(argv[i]);
    }

    if (pl == NULL) {
      fprintf(stderr, "planet %s not found\n", argv[i]);
      return 1;
    }

 state_1:
    if (++i == argc) return 0;
    
    if (!strcmp(argv[i], "get")) {
      get(argv[0], pl);
      goto state_1;
    }

    if (!strcmp(argv[i], "verbose")) {
      verbose++;
      goto state_1;
    }

    if (set_flag(argv[i], "repair", PLREPAIR, pl, verbose)) goto state_1;
    if (set_flag(argv[i], "fuel",   PLFUEL,   pl, verbose)) goto state_1;
    if (set_flag(argv[i], "agri",   PLAGRI,   pl, verbose)) goto state_1;
    if (set_flag(argv[i], "redraw", PLREDRAW, pl, verbose)) goto state_1;
    if (set_flag(argv[i], "home",   PLHOME,   pl, verbose)) goto state_1;
    if (set_flag(argv[i], "coup",   PLCOUP,   pl, verbose)) goto state_1;
    if (set_flag(argv[i], "cheap",  PLCHEAP,  pl, verbose)) goto state_1;
    if (set_flag(argv[i], "core",   PLCORE,   pl, verbose)) goto state_1;

    if (clr_flag(argv[i], "norepair", PLREPAIR, pl, verbose)) goto state_1;
    if (clr_flag(argv[i], "nofuel",   PLFUEL,   pl, verbose)) goto state_1;
    if (clr_flag(argv[i], "noagri",   PLAGRI,   pl, verbose)) goto state_1;
    if (clr_flag(argv[i], "noredraw", PLREDRAW, pl, verbose)) goto state_1;
    if (clr_flag(argv[i], "nohome",   PLHOME,   pl, verbose)) goto state_1;
    if (clr_flag(argv[i], "nocoup",   PLCOUP,   pl, verbose)) goto state_1;
    if (clr_flag(argv[i], "nocheap",  PLCHEAP,  pl, verbose)) goto state_1;
    if (clr_flag(argv[i], "nocore",   PLCORE,   pl, verbose)) goto state_1;

    if (!strcmp(argv[i], "owner")) {
      if (++i == argc) return 0;
      pl->pl_owner = team_find(argv[i]);
      goto state_1;
    }

    if (!strcmp(argv[i], "taken-by")) {
      if (++i == argc) return 0;
      pl->pl_owner = team_find(argv[i]);
      pl->pl_armies = 1;
      pl->pl_info = pl->pl_owner;
      if (verbose) say("%s taken for the %s", pl->pl_name, 
		       team_name(pl->pl_owner));
      goto state_1;
    }

    if (!strcmp(argv[i], "neutral")) {
      pl->pl_armies = 0;
      pl->pl_owner = 0;
      pl->pl_info = ALLTEAM;
      if (verbose) say("%s neutral", pl->pl_name);
      goto state_1;
    }

    if (!strcmp(argv[i], "x")) {
      if (++i == argc) return 0;
      pl->pl_x = atoi(argv[i]);
      goto state_1;
    }

    if (!strcmp(argv[i], "y")) {
      if (++i == argc) return 0;
      pl->pl_y = atoi(argv[i]);
      goto state_1;
    }

    if (!strcmp(argv[i], "restore")) {
      struct planet *virginal = pl_virgin();
      pl->pl_x = virginal[pl->pl_no].pl_x;
      pl->pl_y = virginal[pl->pl_no].pl_y;
      if (verbose) say("%s restored to normal location", pl->pl_name);
      goto state_1;
    }

    /* crawl after a player until slot freed */
    if (!strcmp(argv[i], "crawl-after")) {
      if (++i == argc) return 0;
      struct player *me = player_by_number(argv[i]);
      while (me != NULL) {
	if (me->p_status == PFREE) break;
	if (me->p_flags & PFORBIT)
	  if (me->p_planet == pl->pl_no) continue;
	if (pl->pl_y > me->p_y + 10) pl->pl_y -= 10;
	if (pl->pl_y < me->p_y - 10) pl->pl_y += 10;
	if (pl->pl_x > me->p_x + 10) pl->pl_x -= 10;
	if (pl->pl_x < me->p_x - 10) pl->pl_x += 10;
	usleep(100000);
      }
      goto state_1;
    }

    /* cause a player to visit the planet */
    if (!strcmp(argv[i], "be-orbited-by")) {
      enum be_orbit_by_state { BOBS_BEGIN, BOBS_APPROACH } bobs;
      bobs = BOBS_BEGIN;
      if (++i == argc) return 0;
      struct player *me = player_by_number(argv[i]);
      while (me != NULL) {
	if (me->p_status != PALIVE) break;
	switch (bobs) {
	case BOBS_BEGIN:
	  /* warp on, from set_speed() in interface.c */
	  me->p_desspeed = me->p_ship.s_maxspeed;
	  me->p_flags &= ~(PFREPAIR | PFBOMB | PFORBIT | PFBEAMUP | PFBEAMDOWN);
	  /* lock on, from lock_planet() in interface.c */
	  me->p_flags |= PFPLLOCK;
	  me->p_flags &= ~(PFPLOCK|PFORBIT|PFBEAMUP|PFBEAMDOWN|PFBOMB);
	  me->p_planet = pl->pl_no;
	  bobs = BOBS_APPROACH;
	case BOBS_APPROACH:
	  /* wait until we orbit */
	  if (!(me->p_flags & PFORBIT)) break;
	  exit(0);
	}
	usleep(100000);
      }
      exit(1);
    }

    /* cause a player to take the planet */
    if (!strcmp(argv[i], "be-taken-by")) {
      enum be_taken_by_state { BTBS_BEGIN, BTBS_APPROACH, BTBS_ORBIT, BTBS_DROP } btbs;
      btbs = BTBS_BEGIN;
      if (++i == argc) return 0;
      struct player *me = player_by_number(argv[i]);
      while (me != NULL) {
	if (me->p_status != PALIVE) break;
	switch (btbs) {
	case BTBS_BEGIN:
	  /* kill increment */
	  if (me->p_kills < 4) me->p_kills = 4;
	  /* armies increment */
	  if (me->p_armies < 6) me->p_armies = 6;
	  /* planet flatten */
	  if (pl->pl_armies > 1) pl->pl_armies = 1;
	  /* warp on, from set_speed() in interface.c */
	  me->p_desspeed = me->p_ship.s_maxspeed;
	  me->p_flags &= ~(PFREPAIR | PFBOMB | PFORBIT | PFBEAMUP | PFBEAMDOWN);
	  /* lock on, from lock_planet() in interface.c */
	  me->p_flags |= PFPLLOCK;
	  me->p_flags &= ~(PFPLOCK|PFORBIT|PFBEAMUP|PFBEAMDOWN|PFBOMB);
	  me->p_planet = pl->pl_no;
	  btbs = BTBS_APPROACH;
	case BTBS_APPROACH:
	  /* wait until we orbit */
	  if (!(me->p_flags & PFORBIT)) break;
	  btbs = BTBS_ORBIT;
	case BTBS_ORBIT:
	  /* begin drop */
	  me->p_flags |= PFBEAMDOWN;
	  me->p_flags &= ~(PFSHIELD | PFREPAIR | PFBOMB | PFBEAMUP);
	  btbs = BTBS_DROP;
	case BTBS_DROP:
	  /* wait until planet is ours */
	  if (me->p_team != pl->pl_owner) break;
	  exit(0);
	}
	usleep(100000);
      }
      exit(1);
    }

    if (!strcmp(argv[i], "name")) {
      if (++i == argc) return 0;
      if (verbose) say("%s renamed to %s", pl->pl_name, argv[i]);
      pl->pl_namelen = 0;
      strcpy(pl->pl_name, argv[i]);
      pl->pl_namelen = strlen(pl->pl_name);
      goto state_1;
    }

    if (!strcmp(argv[i], "armies")) {
      if (++i == argc) return 0;
      pl->pl_armies = atoi(argv[i]);
      if (verbose) say("%s armies set to %s", pl->pl_name, argv[i]);
      goto state_1;
    }

    if (!strcmp(argv[i], "flatten")) {
      pl->pl_armies = 1;
      if (verbose) say("%s flattened", pl->pl_name);
      goto state_1;
    }

    if (!strcmp(argv[i], "pop")) {
      pl->pl_armies++;
      if (verbose) say("%s popped to %d", pl->pl_name, pl->pl_armies);
      goto state_1;
    }

    if (!strcmp(argv[i], "plague")) {
      pl->pl_armies--;
      if (verbose) say("%s plagued to %d", pl->pl_name, pl->pl_armies);
      goto state_1;
    }

    if (!strcmp(argv[i], "info")) {
      if (++i == argc) return 0;
      pl->pl_info = atoi(argv[i]);
      goto state_1;
    }

    if (!strcmp(argv[i], "reveal")) {
      pl->pl_info = ALLTEAM;
      goto state_1;
    }

    if (!strcmp(argv[i], "lose")) {
      pl->pl_info = pl->pl_owner;
      goto state_1;
    }

    if (!strcmp(argv[i], "lost")) {
      pl->pl_info = 0;
      goto state_1;
    }

    if (!strcmp(argv[i], "hide")) {
      orbit_release_by_planet(pl);
      pl->pl_x = -10000;
      pl->pl_y = -10000;
      goto state_1;
    }

    if (!strcmp(argv[i], "deorbit")) {
      orbit_release_by_planet(pl);
      goto state_1;
    }

    if (!strcmp(argv[i], "couptime")) {
      if (++i == argc) return 0;
      pl->pl_couptime = atoi(argv[i]);
      goto state_1;
    }

    if (!strcmp(argv[i], "pause")) {
      sleep(1);
      goto state_1;
    }

    if (!strcmp(argv[i], "is-flat")) {
      if (pl->pl_armies < 5) return 0;
      return 1;
    }

    goto state_0;
}

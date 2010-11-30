#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "util.h"
#include "slotmaint.h"

static void Usage(void);

static void Usage(void)
{
  printf("\
    xtkill [0-9a-z] <mode><mode option>\n\
\n\
    where <mode> is one of :\n\
      F(ree slot)                   (bypasses 6 minute ghostbuster timeout)\n\
      e(ject from game)             (simulates self-destruct)\n\
      (no mode == obliterate)\n\
      s(hip class change)[abcdgosSA](S = super SC with 1pt torps, A = ATT)\n\
      t(eleport to quadrant)[frkoc] (c = center of galaxy)\n\
      p(uck)                        (harmless little thing)\n\
      S(uper)                       (big shields/max damage/max etmp)\n\
      T(eam change)[frko]           (no team == independent)\n\
      D(emote)                      (-1 to rank)\n\
      P(romote)                     (+1 to rank)\n\
      k(ills increment)[n]          (+1 kill by default, or +n kills)\n\
      h(arm)                        (no shields, 50%% damage)\n\
      H(ack)                        (cripple, puckify, and freeze player)\n\
      a(rmies increment)[n]         (+6 armies, or set to n)\n\
      u(p shields)                  (raise player's shields)\n\
      d(own shields)                (lower player's shields)\n\
      C(lock, surrender -- set it)  (to 6 minutes (debugging aid))\n\
      L(oss adjust, SB (-1))        (in case you toast an SB accidentally)\n\
      R(obot obliterate)            (like obliterate, but only for robots)\n\
      r(epair)                      (full repair [shields + hull + fuel])\n\
      f(latten)                     (remove kills / armies)\n\
      n(opick) toggle               (toggle ability to pick armies)\n\
");
  exit(1);
}

static void refit(struct player *me, int type)
{
  getship(&(me->p_ship), type);

  /* enable docking */
  if (type == STARBASE) me->p_flags |= PFDOCKOK;

  /* restrict speed to new class */
  if (me->p_desspeed > me->p_ship.s_maxspeed)
    me->p_desspeed = me->p_ship.s_maxspeed;

  /* bump all docked ships */
  bay_release_all(me);
  me->p_flags |= PFDOCKOK;
}

int main(int argc, char **argv)
{
  int player;
  char buf[1000];
  int c;

  if (argc < 2)
    Usage();
  getpath();

  openmem(0);

  player=atoi(argv[1]);
  if ((player == 0) && (*argv[1] != '0')) {
    c = *argv[1];
    if (c >= 'a' && c <= 'z')
      player = c - 'a' + 10;
    else {
      Usage();
    }
  }
  if (player >= MAXPLAYER) {
    printf("MAXPLAYER is set to %d, and you want %d?\n", 
	   MAXPLAYER, player);
    exit(1);
  }

  /* request to free slot? */
  if (argc > 2) {
    if (*argv[2] == 'F') {
      int pid;
      pid = players[player].p_process;
      players[player].p_disconnect = BADVERSION_DENIED;
      usleep(200000); /* guarantee an update will occur */
      if (players[player].p_status == PFREE) exit(0); /* slot free, he went */
      if (pid != players[player].p_process) exit(0); /* pid changed, he went */
      if (players[player].p_process > 1) { /* p_disconnect must have failed */
	if (kill(players[player].p_process, SIGTERM) == 0)
	  exit(0);
      }
      /* if we have no pid_t, or the pid_t was wrong, free slot */
      freeslot(&players[player]);
      exit(0);
    }
  }

  if (players[player].p_status != PALIVE) {
    if(players[player].p_status != POBSERV) {
      printf("Slot is not alive.\n");
      exit(1);
    }
  }

  if (argc > 2)
    switch (*argv[2]) {
    case 'T': { /* change team */
      int team;
	      
      switch (argv[2][1]) {
      case 'f': team = 1; break;
      case 'r': team = 2; break;
      case 'k': team = 4; break;
      case 'o': team = 8; break;
      default:  team = 0;
      }	    
      change_team_quietly(players[player].p_no, team, players[player].p_team);
    }
    break;
    case 'e': /* eject (GHOSTTIME applies in daemon) */
      sprintf(buf, "GOD->ALL  %s has been ejected from the game.", 
	      players[player].p_longname);		    
      players[player].p_whydead=KQUIT;
      players[player].p_explode=10;
      players[player].p_status=3;
      players[player].p_whodead=0;
      amessage(buf, 0, MALL);
      break;
    case 't': /* teleport */
      switch(argv[2][1]) {
      case 'f':
	p_x_y_set(&players[player], planets[0].pl_x, planets[0].pl_y);
	break;
      case 'r':
	p_x_y_set(&players[player], planets[10].pl_x, planets[10].pl_y);
	break;
      case 'k':
	p_x_y_set(&players[player], planets[20].pl_x, planets[20].pl_y);
	break;
      case 'o':
	p_x_y_set(&players[player], planets[30].pl_x, planets[30].pl_y);
	break;
      case 'c':
	p_x_y_set(&players[player], GWIDTH/2, GWIDTH/2);
	break;
      default:
	printf("Valid teleports: frkoc.\n");
	exit(1);
      }
      break;
    case 's': /* ship type change */
      switch (argv[2][1]) {
      case 'a': refit(&players[player], ASSAULT); break;
      case 'b': refit(&players[player], BATTLESHIP); break;
      case 'c': refit(&players[player], CRUISER); break;
      case 'd': refit(&players[player], DESTROYER); break;
      case 'g': refit(&players[player], SGALAXY); break;
      case 's': refit(&players[player], SCOUT); break;
      case 'o': refit(&players[player], STARBASE); break;
      case 'A': refit(&players[player], ATT); break;
      case 'S': 
	getship(&players[player].p_ship, SCOUT);
	players[player].p_ship.s_torpdamage = 1;
	players[player].p_ship.s_torpfuse = 8;
	players[player].p_ship.s_phaserdamage = 1;
	players[player].p_ship.s_plasmadamage = 1;
	players[player].p_ship.s_plasmaspeed = 1;
	players[player].p_ship.s_plasmaturns = 1;
	players[player].p_ship.s_maxshield = 750;
	break;
      default:
	printf("Valid ship types: abcdgsoAS.\n");
	exit(1);
	break;
      }
      players[player].p_damage = 0;
      players[player].p_shield = players[player].p_ship.s_maxshield;
      players[player].p_wtemp = 0;
      players[player].p_etemp = 0;
      players[player].p_fuel = players[player].p_ship.s_maxfuel;
      if (argv[2][1] == 'o') players[player].p_flags |= PFDOCKOK;
      break;
    case 'p':		/* puck? */
      players[player].p_ship.s_tractstr = 1;
      players[player].p_ship.s_torpdamage = -1;
      players[player].p_ship.s_plasmadamage = -1;
      players[player].p_ship.s_phaserdamage = -1;
      players[player].p_hostile = 0;
      players[player].p_swar = 0;
      players[player].p_war = 0;
      players[player].p_team = 0;	/* indep */
      players[player].p_ship.s_type = SCOUT;
      players[player].p_ship.s_mass = 200;
      players[player].p_ship.s_repair = 30000;
      break;
    case 'S':		/* super ship */
      players[player].p_ship.s_maxshield = 750;
      players[player].p_shield = 750;
      players[player].p_ship.s_maxdamage = 750;
      players[player].p_damage = 0;
      players[player].p_ship.s_maxegntemp = 5000;
      players[player].p_etemp = 0;
      break;
    case 'D':		/* demote, but not beyond ensign */
      if(players[player].p_stats.st_rank == 0)
	players[player].p_stats.st_rank++;

      --players[player].p_stats.st_rank;
      break;
    case 'P':		/* promote, but not beyond admiral */
      if( players[player].p_stats.st_rank < (NUMRANKS - 1) ) 
	++players[player].p_stats.st_rank;
      break;
    case 'k':		/* kill increment */
      if (strlen(argv[2]) > 1)
        players[player].p_kills += atoi(argv[2]+1);
      else
        players[player].p_kills += 1.0;
      break;
    case 'a':		/* army increment */
      if (strlen(argv[2]) > 1) 
	players[player].p_armies = atoi(argv[2]+1);
      else 
	players[player].p_armies += 6;
      break;
    case 'C':		/* clock surrender reset */
      teams[players[player].p_team].te_surrender = 6;
      break;
    case 'h':		/* harm */
      players[player].p_shield = 0;
      players[player].p_damage = players[player].p_ship.s_maxdamage/2;
      break;
    case 'H':           /* hack */
      {
      struct player *me = &players[player];
      /* make independent and hostile to only prior team */
      int team = players[player].p_team;
      players[player].p_hostile = team;
      players[player].p_swar = team;
      players[player].p_war = team;
      players[player].p_team = 0;
      sprintf(players[player].p_mapchars, "%c%c", 
	      teamlet[players[player].p_team], shipnos[player]);
      sprintf(players[player].p_longname, "%s (%s)", 
	      players[player].p_name, players[player].p_mapchars);
      /* cripple */
      players[player].p_shield = 0;
      players[player].p_damage = players[player].p_ship.s_maxdamage/2;
      /* raise shields */
      players[player].p_flags |= PFSHIELD;
      players[player].p_flags &= ~(PFBOMB | PFREPAIR | PFBEAMUP | PFBEAMDOWN);
      /* break tractors and decloak */
      me->p_flags &= ~(PFTRACT | PFPRESS);
      me->p_flags &= ~PFCLOAK;
      /* set speed 0 */
      me->p_desspeed = 0;
      bay_release(me);
      me->p_flags &= ~(PFREPAIR | PFBOMB | PFORBIT | PFBEAMUP | PFBEAMDOWN);
      /* make unable to act */
      players[player].p_flags |= PFTWARP;
      /* show as puck */
      players[player].p_ship.s_type = SCOUT;
      }
      break;
    case 'u':           /* raise shields */
      players[player].p_flags |= PFSHIELD;
      players[player].p_flags &= ~(PFBOMB | PFREPAIR | PFBEAMUP | PFBEAMDOWN);
      break;
    case 'd':           /* lower shields */
      players[player].p_flags &= ~(PFSHIELD);
      break;
    case 'R':		/* robot kill? */
      if (players[player].p_flags & PFROBOT) {
	players[player].p_ship.s_type = STARBASE;
	players[player].p_whydead=KPROVIDENCE;
	players[player].p_explode=10;
	players[player].p_status=3;
	players[player].p_whodead=0;
      }
      break;
    case 'L':		/* starbase loss adjust */
#ifdef LTD_STATS
#ifndef LTD_PER_RACE
      players[player].p_stats.ltd[0][LTD_SB].deaths.total--;
#endif
#else
      players[player].p_stats.st_sblosses--;
#endif
      break;
    case 'r':           /* repair ship */
      p_heal(&players[player]);
      break;
    case 'f':		/* flatten kills / armies */
      players[player].p_kills = 0;
      players[player].p_armies = 0;
      break;
    case 'n':		/* toggle carry flag */
      players[player].p_no_pick = !players[player].p_no_pick;
      sprintf(buf, "GOD->%s  %s %s pick up armies",
              team_code(players[player].p_team),
              players[player].p_longname,
              players[player].p_no_pick ?
                  "is no longer able to" : "can once again");
      amessage(buf, players[player].p_team, MTEAM);
      break;
    default:
      Usage();
    }			/* end switch */
  else {
    sprintf(buf, "GOD->ALL  %s was utterly obliterated.", 
	    players[player].p_longname);
    players[player].p_ship.s_type=STARBASE;
    players[player].p_whydead=KPROVIDENCE;
    players[player].p_explode=10;
    players[player].p_status=3;
    players[player].p_whodead=0;
    amessage(buf, 0, MALL);
  }
  return 0;
}

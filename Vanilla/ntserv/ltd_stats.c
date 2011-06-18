/* $Id: ltd_stats.c,v 1.1 2005/03/21 05:23:43 jerub Exp $
 *
 * Dave Ahn
 *
 * LTD stats access functions.  For detailed info, read the README.LTD
 * file.
 *
 * The LTD stats should be accessed and modified using the access
 * functions if at all possible.  Depending on the type of compiler,
 * these should or should not be declared inline.  For example, on the
 * SGI MIPSpro compilers, the -ipa option will do interprodecure
 * optimizations (i.e. functions will get inlined even if they are
 * declared in another file).  Others, like gcc, aren't so smart.
 * Depending on the level of inlining that is needed (e.g.  you may be
 * on a slow sparcstation), the LTD_INLINE define should be changed.
 *
 * */

#include "config.h"

#ifdef LTD_STATS

#include <stdio.h>
#include <string.h>
#include "ltd_stats.h"
#include "struct.h"
#include "data.h"

#ifndef LTD_INLINE 		/* if all ltd access functions are inlined,
                                   this file should be compiled with only
                                   non-inlined functions declared */


/* COMPAT functions to calculate old-style ratings */
 
float ltd_total_rating(struct player *p) {
  return (ltd_bombing_rating(p) +
          ltd_planet_rating(p) +
          ltd_offense_rating(p));
}


float ltd_bombing_rating(struct player *p) {

  return ((float) ltd_armies_bombed(p, LTD_TOTAL) * status->timeprod /
          ((float) ltd_ticks(p, LTD_TOTAL) * status->armsbomb));

}

float ltd_planet_rating(struct player *p) {

  return ((float) ltd_planets_taken(p, LTD_TOTAL) * status->timeprod /
	 ((float) ltd_ticks(p, LTD_TOTAL) * status->planets));
}

float ltd_defense_rating(struct player *p) {

  return ((float) ltd_ticks(p, LTD_TOTAL) * status->losses /
	 ((ltd_deaths(p, LTD_TOTAL) != 0) ?
          ((float) ltd_deaths(p, LTD_TOTAL) * status->timeprod) :
          status->timeprod));

}

float ltd_offense_rating(struct player *p) {

  return ((float) ltd_kills(p, LTD_TOTAL) * status->timeprod /
	 ((float) ltd_ticks(p, LTD_TOTAL) * status->kills));
}

int ltd_kills(struct player *p, LTD_SHIP_T s) {

  if (s == LTD_TOTAL) {

    int race, ship, total = 0;

    /* forced update the LTD_TOTAL slot on demand */
    for (race=0; race<LTD_NUM_RACES; race++) {
      for (ship=1; ship<LTD_NUM_SHIPS; ship++) {
        if (ship != LTD_SB)
          total += p->p_stats.ltd[race][ship].kills.total;
      }
      p->p_stats.ltd[race][LTD_TOTAL].kills.total = total;
    }

  }

  return (p->p_stats.ltd[ltd_race(p->p_team)][s].kills.total);
}

int ltd_deaths(struct player *p, LTD_SHIP_T s) {

  if (s == LTD_TOTAL) {

    int race, ship, total = 0;

    /* forced update the LTD_TOTAL slot on demand */
    for (race=0; race<LTD_NUM_RACES; race++) {
      for (ship=1; ship<LTD_NUM_SHIPS; ship++) {
        if (ship != LTD_SB)
          total += p->p_stats.ltd[race][ship].deaths.total;
      }
      p->p_stats.ltd[race][LTD_TOTAL].deaths.total = total;
    }

  }

  return (p->p_stats.ltd[ltd_race(p->p_team)][s].deaths.total);
}

int ltd_armies_bombed(struct player *p, LTD_SHIP_T s) {

  if (s == LTD_TOTAL) {

    int race, ship, total = 0, ogg_total = 0;

    /* forced update the LTD_TOTAL slot on demand */
    for (race=0; race<LTD_NUM_RACES; race++) {
      for (ship=1; ship<LTD_NUM_SHIPS; ship++) {
        if (ship != LTD_SB) {
          total += p->p_stats.ltd[race][ship].bomb.armies;
          ogg_total += p->p_stats.ltd[race][ship].ogged.armies;
        }
      }
      p->p_stats.ltd[race][LTD_TOTAL].bomb.armies = total;
      p->p_stats.ltd[race][LTD_TOTAL].ogged.armies = ogg_total;
    }

  }

  return (p->p_stats.ltd[ltd_race(p->p_team)][s].bomb.armies +
          5 * p->p_stats.ltd[ltd_race(p->p_team)][s].ogged.armies);
}

int ltd_planets_taken(struct player *p, LTD_SHIP_T s) {

  if (s == LTD_TOTAL) {

    int race, ship, total = 0;

    /* forced update the LTD_TOTAL slot on demand */
    for (race=0; race<LTD_NUM_RACES; race++) {
      for (ship=1; ship<LTD_NUM_SHIPS; ship++) {
        if (ship != LTD_SB)
          total += p->p_stats.ltd[race][ship].planets.taken;
      }
      p->p_stats.ltd[race][LTD_TOTAL].planets.taken = total;

    }
  }

  return (p->p_stats.ltd[ltd_race(p->p_team)][s].planets.taken);
}

int ltd_ticks(struct player *p, LTD_SHIP_T s) {

  if (s == LTD_TOTAL) {

    int race, ship, total = 0;

    /* forced update the LTD_TOTAL slot on demand */

    for (race=0; race<LTD_NUM_RACES; race++) {
      for (ship=1; ship<LTD_NUM_SHIPS; ship++) {
        if (ship != LTD_SB)
          total += p->p_stats.ltd[race][ship].ticks.total;
      }
      p->p_stats.ltd[race][LTD_TOTAL].ticks.total = total;
    }

  }

  return (p->p_stats.ltd[ltd_race(p->p_team)][s].ticks.total);
}

int ltd_kills_max(struct player *p, LTD_SHIP_T s) {

  if (s == LTD_TOTAL) {

    int race, ship, max = 0;

    /* forced update the LTD_TOTAL slot on demand */

    for (race=0; race<LTD_NUM_RACES; race++) {
      for (ship=1; ship<LTD_NUM_SHIPS; ship++) {
        if (ship != LTD_SB)
          if (p->p_stats.ltd[race][ship].kills.max > max)
            max = p->p_stats.ltd[race][ship].kills.max;
      }
      p->p_stats.ltd[race][LTD_TOTAL].kills.max = max;
    }

  }

  return (p->p_stats.ltd[ltd_race(p->p_team)][s].kills.max);
}

struct _ltd_ship {
  int index;
  char name[3];
};

static const struct _ltd_ship ltd_ship[NUM_TYPES] = {

  /* order is important! */
  { LTD_SC, "SC" },	/* SCOUT */
  { LTD_DD, "DD" },	/* DESTROYER */
  { LTD_CA, "CA" },	/* CRUISER */
  { LTD_BB, "BB" },	/* BATTLESHIP */
  { LTD_AS, "AS" },	/* ASSAULT */
  { LTD_SB, "SB" },	/* STARBASE */
  { LTD_GA, "GA" },	/* GALAXY or ATT */
  { LTD_TOTAL, "TL" },	/* total */

};


/* galaxy width = 100000
   home = 20000, 80000
   Fed: x: (0, 50000)
        y: (50000, 100000)
   Rom: x: (0, 50000)
        y: (0, 50000)
   Kli: x: (50000, 100000)
        y: (0, 50000)
   Ori: x: (50000, 100000)
        y: (50000, 100000)
 */

/* ltd_zone assumes the two teams in play are adjacent */

inline static LTD_TZONE_T ltd_zone(struct player *p) {

  const int x = p->p_x;
  const int y = p->p_y;

  const int other_team = p->p_hist.enemy_team;

  if (p->p_status != PALIVE) return LTD_TZONE_7;

  switch(p->p_team) {
    case FED:
      switch(other_team) {
        case ROM:
          if (x < 50000) {
            if (y >= 80000) return LTD_TZONE_0;	/* backfield */
            if (y >= 65000) return LTD_TZONE_1;	/* core + 2 side */
            if (y >= 50000) return LTD_TZONE_2;	/* front line */
            if (y >= 35000) return LTD_TZONE_3;	/* enemy front line */
            if (y >= 20000) return LTD_TZONE_4;	/* enemy core + 2 side */
            return LTD_TZONE_5;			/* enemy backfield */
          }
          return LTD_TZONE_6;			/* 3rd space */
        case ORI:
          if (y >= 50000) {
            if (x < 20000) return LTD_TZONE_0;	/* backfield */
            if (x < 35000) return LTD_TZONE_1;	/* core + 2 side */
            if (x < 50000) return LTD_TZONE_2;	/* front line */
            if (x < 65000) return LTD_TZONE_3;	/* enemy front line */
            if (x < 80000) return LTD_TZONE_4;	/* enemy core + 2 side */
            return LTD_TZONE_5;			/* enemy backfield */
          }
          return LTD_TZONE_6;			/* 3rd space */
        default:
          return LTD_TZONE_7;
      }
    case ROM:
      switch(other_team) {
        case FED:
          if (x < 50000) {
            if (y < 20000) return LTD_TZONE_0;	/* backfield */
            if (y < 35000) return LTD_TZONE_1;	/* core + 2 side */
            if (y < 50000) return LTD_TZONE_2;	/* front line */
            if (y < 65000) return LTD_TZONE_3;	/* enemy front line */
            if (y < 80000) return LTD_TZONE_4;	/* enemy core + 2 side */
            return LTD_TZONE_5;			/* enemy backfield */
          }
          return LTD_TZONE_6;			/* 3rd space */
        case KLI:
          if (y < 50000) {
            if (x < 20000) return LTD_TZONE_0;	/* backfield */
            if (x < 35000) return LTD_TZONE_1;	/* core + 2 side */
            if (x < 50000) return LTD_TZONE_2;	/* front line */
            if (x < 65000) return LTD_TZONE_3;	/* enemy front line */
            if (x < 80000) return LTD_TZONE_4;	/* enemy core + 2 side */
            return LTD_TZONE_5;			/* enemy backfield */
          }
          return LTD_TZONE_6;			/* 3rd space */
        default:
          return LTD_TZONE_7;
      }
    case KLI:
      switch(other_team) {
        case ROM:
          if (y < 50000) {
            if (x >= 80000) return LTD_TZONE_0;	/* backfield */
            if (x >= 65000) return LTD_TZONE_1;	/* core + 2 side */
            if (x >= 50000) return LTD_TZONE_2;	/* front line */
            if (x >= 35000) return LTD_TZONE_3;	/* enemy front line */
            if (x >= 20000) return LTD_TZONE_4;	/* enemy core + 2 side */
            return LTD_TZONE_5;			/* enemy backfield */
          }
          return LTD_TZONE_6;			/* 3rd space */
        case ORI:
          if (x >= 50000) {
            if (y < 20000) return LTD_TZONE_0;	/* backfield */
            if (y < 35000) return LTD_TZONE_1;	/* core + 2 side */
            if (y < 50000) return LTD_TZONE_2;	/* front line */
            if (y < 65000) return LTD_TZONE_3;	/* enemy front line */
            if (y < 80000) return LTD_TZONE_4;	/* enemy core + 2 side */
            return LTD_TZONE_5;			/* enemy backfield */
          }
          return LTD_TZONE_6;			/* 3rd space */
        default:
          return LTD_TZONE_7;
      }
    case ORI:
      switch(other_team) {
        case FED:
          if (y >= 50000) {
            if (x >= 80000) return LTD_TZONE_0;	/* backfield */
            if (x >= 65000) return LTD_TZONE_1;	/* core + 2 side */
            if (x >= 50000) return LTD_TZONE_2;	/* front line */
            if (x >= 35000) return LTD_TZONE_3;	/* enemy front line */
            if (x >= 20000) return LTD_TZONE_4;	/* enemy core + 2 side */
            return LTD_TZONE_5;			/* enemy backfield */
          }
          return LTD_TZONE_6;			/* 3rd space */
        case KLI:
          if (x >= 50000) {
            if (y >= 80000) return LTD_TZONE_0;	/* backfield */
            if (y >= 65000) return LTD_TZONE_1;	/* core + 2 side */
            if (y >= 50000) return LTD_TZONE_2;	/* front line */
            if (y >= 35000) return LTD_TZONE_3;	/* enemy front line */
            if (y >= 20000) return LTD_TZONE_4;	/* enemy core + 2 side */
            return LTD_TZONE_5;			/* enemy backfield */
          }
          return LTD_TZONE_6;			/* 3rd space */
        default:
          return LTD_TZONE_7;
      }
    default:
      return LTD_TZONE_7;
  }

}

static struct ltd_stats *ltd_stat_ptr_by_ship(struct player *p, int ship_index)
{
  if (status->tourn) {
    return &p->p_stats.ltd[ltd_race(p->p_team)][ship_index];
  } else {
    return &p->p_bogus;
  }
}

struct ltd_stats *ltd_stat_ptr(struct player *p)
{
  return ltd_stat_ptr_by_ship(p, ltd_ship[p->p_ship.s_type].index);
}

struct ltd_stats *ltd_stat_ptr_total(struct player *p)
{
  return ltd_stat_ptr_by_ship(p, LTD_TOTAL);
}

void ltd_update_ticks(struct player *p) {

  u_int pflags = p->p_flags;
  struct ltd_stats *ltd = ltd_stat_ptr(p);

  /* LTD_TOTAL ticks */
/*  p->p_stats.ltd[LTD_TOTAL].ticks.total++;*/

  /* total ticks */
  ltd->ticks.total++;

  /* red alert */
  if (pflags & PFRED) {

    ltd->ticks.red++;

  }
  /* yellow alert */
  else if (pflags & PFYELLOW) {

    ltd->ticks.yellow++;

  }

  /* carrier */
  if (p->p_armies > 0) {

    ltd->ticks.carrier++;

  }
  /* potential++ only if not carrying armies.  in terms of ticks, time
     spent as a potential carrier and as converted carrier are merged */

  else if (p->p_hist.kill_potential) {

    ltd->ticks.potential++;

  }

  /* repair mode */
  if (pflags & PFREPAIR) {

    ltd->ticks.repair++;

  }

  /* zone update every 10 ticks */
  if ((ltd->ticks.total % 10) == 0) {

    ltd->ticks.zone[ltd_zone(p)]++;

  }

  /* update history */

  if ((p->p_hist.kill_potential == 0) &&
      (p->p_hist.num_kills > 0) &&
      (pflags & PFGREEN) &&
      (p->p_damage < p->p_ship.s_maxdamage / 2)) {

    /* reached green alert with less than 50% hull damage with at
       least one kill without already being a potential or converted
       carrier */

    ltd->kills.first_potential++;

    /* i am a potential 1st kill */
    p->p_hist.kill_potential = 1;

  }

  /* second kill potential is separate from first kill potential because
     the second kill can be acquired long after the first kill */
  if ((p->p_hist.kill_potential == 1) &&
      (p->p_hist.num_kills > 1) &&
      (pflags & PFGREEN) &&
      (p->p_damage < p->p_ship.s_maxdamage / 2)) {

    ltd->kills.second_potential++;

    /* i am a potential 2nd kill */
    p->p_hist.kill_potential = 2;

  }

}


void ltd_update_kills(struct player *credit_killer,
                      struct player *actual_killer,
                      struct player *victim) {

  short killer_ship = credit_killer->p_ship.s_type;
  short victim_ship = victim->p_ship.s_type;
  short victim_armies = victim->p_armies;

  struct ltd_stats *killer_ltd = ltd_stat_ptr(credit_killer);
  struct ltd_stats *actual_ltd = ltd_stat_ptr(actual_killer);

  struct ltd_history *killer_hist = &(credit_killer->p_hist);
  int kill_num;

  /* total kills */
  killer_ltd->kills.total++;

  /* kill by weapon type */
  switch(victim->p_whydead) {

    case KPHASER:
      killer_ltd->kills.phasered++;
      break;
    case KTORP:
    case KTORP2:
      killer_ltd->kills.torped++;
      break;
    case KPLASMA:
    case KPLASMA2:
      killer_ltd->kills.plasmaed++;
      break;
  }

  /* victim was ++ */
  if (victim_armies > 0) {

    /* handle dooshed SB armies differently */
    if (victim_ship == STARBASE) {

      killer_ltd->ogged.sb_armies += victim_armies;

    }
    /* victim was a non-SB carrier */
    else {

      killer_ltd->ogged.armies += victim_armies;
      killer_ltd->ogged.dooshed++;

    }

    /* twink blew up his own friendly carrier ++ */
    if (actual_killer->p_team == victim->p_team) {

      actual_ltd->ogged.friendly_armies += victim_armies;
      actual_ltd->ogged.friendly++;

    }

    /* killer ship vs victim ship, GA and AT are not counted
     * {SC,DD} < {AS} < {CA,BB} < {SB} */

    switch(killer_ship) {

      case SCOUT:
      case DESTROYER:
        switch(victim_ship) {
          case SCOUT:
          case DESTROYER:
            killer_ltd->ogged.same_ship++;
            break;
          case ASSAULT:
          case CRUISER:
          case BATTLESHIP:
          case STARBASE:
            killer_ltd->ogged.bigger_ship++;
            break;
        }
        break;

      case ASSAULT:
        switch(victim_ship) {
          case SCOUT:
          case DESTROYER:
            killer_ltd->ogged.smaller_ship++;
            break;
          case ASSAULT:
            killer_ltd->ogged.same_ship++;
            break;
          case CRUISER:
          case BATTLESHIP:
          case STARBASE:
            killer_ltd->ogged.bigger_ship++;
            break;
        }
        break;

      case CRUISER:
      case BATTLESHIP:
        switch(victim_ship) {
          case SCOUT:
          case DESTROYER:
          case ASSAULT:
            killer_ltd->ogged.smaller_ship++;
            break;
          case CRUISER:
          case BATTLESHIP:
            killer_ltd->ogged.same_ship++;
            break;
          case STARBASE:
            killer_ltd->ogged.bigger_ship++;
            break;
        }
        break;
        
      case STARBASE:
        switch(victim_ship) {
          case SCOUT:
          case DESTROYER:
          case ASSAULT:
          case CRUISER:
          case BATTLESHIP:
            killer_ltd->ogged.smaller_ship++;
            break;
          case STARBASE:
            killer_ltd->ogged.same_ship++;
            break;
        }
        break;
    }

  } /* victim_armies > 0 */

  else {

    /* victim had no armies.  check for killing potential or converted
       carrier */

    if ((victim->p_hist.kill_potential == 1) ||
        (victim->p_hist.kill_potential == 2)) {

      killer_ltd->ogged.potential++;

    }
    else if (victim->p_hist.kill_potential > 2) {

      killer_ltd->ogged.converted++;

    }
  }

  /* update the ltd history */

  kill_num = killer_hist->num_kills;

  if (kill_num < LTD_NUM_HIST) {

    /* in T mode, ltd->ticks.total is updated every tick, so we can
       use it as a counter */

    killer_hist->kills[kill_num].tick = killer_ltd->ticks.total;
    killer_hist->kills[kill_num].potential = 0;
    killer_hist->kills[kill_num].victim = victim;
    killer_hist->kills[kill_num].before = credit_killer->p_kills  -
      (1.0 + victim->p_armies * 0.1 + victim->p_kills * 0.1);
    killer_hist->kills[kill_num].ship = victim->p_ship.s_type;

    /* this is first kill */
    if (kill_num == 0) {
      killer_ltd->kills.first++;
    }
    /* this is second kill */
    else if (kill_num == 1) {
      killer_ltd->kills.second++;
    }

    /* update the number of last kills */
    killer_hist->num_kills++;

  }

  /* update the max kill count for the credited killer */
  ltd_update_kills_max(credit_killer);
}

void ltd_update_kills_max(struct player *killer) {

  struct ltd_stats *ltd = ltd_stat_ptr(killer);

  if (killer->p_kills > ltd->kills.max) {
    ltd->kills.max = killer->p_kills;
  }

}


void ltd_update_deaths(struct player *victim, struct player *actual_killer) {

  struct ltd_stats *victim_ltd = ltd_stat_ptr(victim);

  /* update the total counter */
  victim_ltd->deaths.total++;

  /* if victim was carrying, update the dooshed counter */
  if (victim->p_armies > 0) {

    victim_ltd->deaths.dooshed++;
    victim_ltd->armies.killed += victim->p_armies;

    /* if victim was beaming the armies down, it was a partial carry */
    if (victim->p_flags & PFBEAMDOWN) {

      victim_ltd->carries.partial++;

    }

  }
  else {
    /* victim had no armies, check to see if victim was a potential or
       converted carrier */

    if ((victim->p_hist.kill_potential == 1) ||
        (victim->p_hist.kill_potential == 2)) {

      victim_ltd->deaths.potential++;

    } else if (victim->p_hist.kill_potential > 2) {

      victim_ltd->deaths.converted++;

    }
  }


  /* victim was killed by a planet */
  if (actual_killer == NULL) return;

  /* how was i killed? */

  switch(victim->p_whydead) {
    case KPHASER:
      victim_ltd->deaths.phasered++;
      break;
    case KTORP:
    case KTORP2:
      victim_ltd->deaths.torped++;
      break;
    case KPLASMA:
    case KPLASMA2:
      victim_ltd->deaths.plasmaed++;
      break;
  }

}


/* bomber = player who bombed
   planet_bombed = planet that was just bombed (i.e. after the armies have
                   been removed from the planet this tick)
   armies_bombed = number of armies that were just bombed off the planet */

void ltd_update_bomb(struct player *bomber, struct planet *planet_bombed,
                     int armies_bombed) {

  struct ltd_stats *bomber_ltd = ltd_stat_ptr(bomber);
  char new_planet = 0;

  /* check and update ltd history */

  /* if this is a new planet, or if 5 seconds have passed after bombing
     the same planet, consider it as new */

  if ((bomber->p_hist.last_bombed_planet != bomber->p_planet) ||
      ((bomber_ltd->ticks.total - bomber->p_hist.last_bombed_tick) > 50)) {

    /* last planet was different */
    bomber->p_hist.last_bombed_planet = bomber->p_planet;
    bomber->p_hist.last_bombed_tick = bomber_ltd->ticks.total;
    bomber_ltd->bomb.planets++;
    new_planet = 1;

  }

  /* total armies bombed */
  bomber_ltd->bomb.armies += armies_bombed;

  /* armies bombed in core */
  if (planet_bombed->pl_flags & PLCORE) {
    bomber_ltd->bomb.armies_core += armies_bombed;

    if (new_planet) {
      bomber_ltd->bomb.planets_core++;
    }
  }

  /* armies bombed on planet <= 8 armies */
  if (planet_bombed->pl_armies + armies_bombed <= 8) {
    bomber_ltd->bomb.armies_8 += armies_bombed;

    if (new_planet) {
      bomber_ltd->bomb.planets_8++;
    }
  }

}


void ltd_update_planets(struct player *taker, struct planet *planet) {

  struct ltd_stats *taker_ltd = ltd_stat_ptr(taker);

  /* this planet was neuted */
  if (planet->pl_owner == NOBODY) {

    taker_ltd->planets.destroyed++;

  }
  /* otherwise, it was taken */
  else {

    taker_ltd->planets.taken++;

  }

}


/* ltd_update_armies is for BEAMDOWN armies to a planet only.  NOTE:
   ltd_update_armies is called AFTER the army has already been
   dropped, so if the enemy had 1 army before the beamdown, it should
   be neutral now. */

void ltd_update_armies(struct player *carrier, struct planet *planet) {

  struct ltd_stats *carrier_ltd = ltd_stat_ptr(carrier);

  /* if player has no armies on board, then the carry was completed */
  if (carrier->p_armies == 0) {

    carrier_ltd->carries.completed++;

  }

  /* if it is an enemy or neuted planet, the army was used to attack it */
  if (planet->pl_owner != carrier->p_team) {

    carrier_ltd->armies.attack++;

    if (carrier->p_hist.last_beamdown_planet != carrier->p_planet) {

      carrier->p_hist.last_beamdown_planet = carrier->p_planet;
      carrier_ltd->carries.attack++;

    }

  }
  /* this is a friendly planet */
  else {

    /* army used to reinforce, good! */
    if (planet->pl_armies <= 4) {

      carrier_ltd->armies.reinforce++;

      if (carrier->p_hist.last_beamdown_planet != carrier->p_planet) {

        carrier->p_hist.last_beamdown_planet = carrier->p_planet;
        carrier_ltd->carries.reinforce++;

      }

    }
    /* otherwise, it was a ferry */
    else {

      /* planet ferried to is the same planet picked from, so negative
         credit */
      if (carrier->p_hist.last_beamup_planet == carrier->p_planet) {

        carrier_ltd->armies.total--;

      }
      /* this was a real ferry to a different planet */
      else {

        carrier_ltd->armies.ferries++;

        if (carrier->p_hist.last_beamdown_planet != carrier->p_planet) {

          carrier->p_hist.last_beamdown_planet = carrier->p_planet;
          carrier_ltd->carries.ferries++;

        }

      }

    }
  }
}


/* ltd_update_armies_carried is for BEAMUP armies only.  NOTE:
   this is called AFTER the army has been beamed up. */

void ltd_update_armies_carried(struct player *carrier, struct player *sb) {

  struct ltd_stats *carrier_ltd = ltd_stat_ptr(carrier);
  struct ltd_stats *sb_ltd;
  int num_kills;

  /* if carrier tries to scum stats by picking armies up and down from
     one planet, ltd_update_armies should catch it */

  carrier_ltd->armies.total++;

  /* update ltd history */

  num_kills = carrier->p_hist.num_kills;

  /* if i'm not already a potential carrier but acquired real kills,
     credit potential carrier stats */

  /* i was not yet a potential carrier */
  if (carrier->p_hist.kill_potential == 0) {

    /* if i had at least one kill, give 1st kill potential */
    if (num_kills >= 1) {
      carrier_ltd->kills.first_potential++;
    }

    /* if i had at least two kills, give 2nd kill potential.  2nd kill
       converted stats keeps track of whether i actually utilized my
       second kill */
    if (num_kills >= 2) {
      carrier_ltd->kills.second_potential++;
    }

    /* i am now a converted 1st kill carrier */
    carrier->p_hist.kill_potential = 3;
  }
  /* i am a potential carrier */
  else if ((carrier->p_hist.kill_potential == 1) ||
           (carrier->p_hist.kill_potential == 2)) {

    if (num_kills > 0) {

      carrier_ltd->kills.first_converted++;

      /* i am now a converted 1st kill carrier */
      carrier->p_hist.kill_potential = 3;

    }
  }
  /* i am a converted 1st kill carrier */
  else if (carrier->p_hist.kill_potential == 3) {

    if ((num_kills > 1) &&
        (carrier->p_armies > (carrier->p_ship.s_type == ASSAULT)? 3: 2)) {

      carrier_ltd->kills.second_converted++;

      /* i am now a converted 2nd kill carrier */
      carrier->p_hist.kill_potential = 4;

    }
  }

  /* update acc */
  if (num_kills > 0) {

    int twink, victim_shiptype;
    float armies;
    struct player *victim;

    armies = (carrier->p_ship.s_type == ASSAULT)? 3.0f: 2.0f;

    for (twink=0; twink<num_kills; twink++) {

      victim = carrier->p_hist.kills[twink].victim;

      if (victim) {

        victim_shiptype = carrier->p_hist.kills[twink].ship;

        if (carrier->p_armies >
            (int) (carrier->p_hist.kills[twink].before * armies)) {

          victim->p_stats.ltd[ltd_race(victim->p_team)]
            [ltd_ship[victim_shiptype].index].deaths.acc++;
          carrier->p_hist.kills[twink].victim = NULL;

          break;

        }
      }
    }
  }

  /* beamup is from a planet */
  if (sb == NULL) {

    /* carrier is beaming from a new planet */
    if (carrier->p_hist.last_beamup_planet != carrier->p_planet) {

      /* update last_beamup_planet */
      carrier->p_hist.last_beamup_planet = carrier->p_planet;

      /* if this is the first army beamed up, update carries.total */
      if (carrier->p_armies == 1) {
        carrier_ltd->carries.total++;
      }

    }

    /* we're done */
    return;
  }

  /* update the SB stats - since SB's can't take planets, we'll use
     the armies.attack field for armies delivered */

  sb_ltd = ltd_stat_ptr(sb);

  /* if carrier tries to scum stats by beaming armies up and down on
     the SB, ltd_update_armies_ferried should catch it */

  sb_ltd->armies.attack++;

  /* update last_beamup_planet */
  if (carrier->p_hist.last_beamup_planet != -1) {

    /* -1 = SB */
    carrier->p_hist.last_beamup_planet = -1;

    /* if this is the first army beamed up, update carries.total */
    if (carrier->p_armies == 1) {
      carrier_ltd->carries.total++;
    }

  }

}


/* ltd_update_armies_ferried is for BEAMDOWN armies to SB only.  This is
   called AFTER the army has been beamed down */

void ltd_update_armies_ferried(struct player *carrier, struct player *sb) {
  
  struct ltd_stats *carrier_ltd = ltd_stat_ptr(carrier);
  struct ltd_stats *sb_ltd = ltd_stat_ptr(sb);

  /* army was beamed up from SB, so no credit */
  if (carrier->p_hist.last_beamup_planet == -1) {

    carrier_ltd->armies.total--;
    sb_ltd->armies.attack--;

  }
  /* army was beamed up from elsewhere, so give credit */
  else {

    carrier_ltd->armies.ferries++;
    sb_ltd->armies.total++;

  }

  /* if the carry was completed, give credit */
  if (carrier->p_armies == 0) {

    carrier_ltd->carries.completed++;

  }

}

/* ltd_update_repaired updates amount of damage that player has
   repaired to shields or hull */

void ltd_update_repaired(struct player *player, const int damage) {

  struct ltd_stats *ltd = ltd_stat_ptr(player);

  ltd->damage_repaired += damage;

}

/* phaser fired */
void ltd_update_phaser_fired(struct player *shooter) {

  struct ltd_stats *ltd = ltd_stat_ptr(shooter);

  ltd->weapons.phaser.fired++;

}

/* phaser hit */
void ltd_update_phaser_hit(struct player *shooter) {

  struct ltd_stats *ltd = ltd_stat_ptr(shooter);

  ltd->weapons.phaser.hit++;

}

/* phaser damage */
void ltd_update_phaser_damage(struct player *shooter, struct player *victim,
                              const int damage) {

  struct ltd_stats *shooter_ltd = ltd_stat_ptr(shooter);
  struct ltd_stats *victim_ltd = ltd_stat_ptr(victim);

  shooter_ltd->weapons.phaser.damage.inflicted += damage;
  victim_ltd->weapons.phaser.damage.taken += damage;

}

/* torp fired */
void ltd_update_torp_fired(struct player *shooter) {

  struct ltd_stats *ltd = ltd_stat_ptr(shooter);

  ltd->weapons.torps.fired++;

}

/* torp hit player */
void ltd_update_torp_hit(struct player *shooter) {

  struct ltd_stats *ltd = ltd_stat_ptr(shooter);

  ltd->weapons.torps.hit++;

}

/* torp hit wall */
void ltd_update_torp_wall(struct player *shooter) {

  struct ltd_stats *ltd = ltd_stat_ptr(shooter);

  ltd->weapons.torps.wall++;

}

/* torp detted by victim */
void ltd_update_torp_detted(struct player *shooter) {

  struct ltd_stats *ltd = ltd_stat_ptr(shooter);

  ltd->weapons.torps.detted++;

}

/* torp self detted by shooter */
void ltd_update_torp_selfdetted(struct player *shooter) {

  struct ltd_stats *ltd = ltd_stat_ptr(shooter);

  ltd->weapons.torps.selfdetted++;

}

/* torp damage */
void ltd_update_torp_damage(struct player *shooter, struct player *victim,
                            const int damage) {

  struct ltd_stats *shooter_ltd = ltd_stat_ptr(shooter);
  struct ltd_stats *victim_ltd = ltd_stat_ptr(victim);

  shooter_ltd->weapons.torps.damage.inflicted += damage;
  victim_ltd->weapons.torps.damage.taken += damage;

}

/* plasma fired */
void ltd_update_plasma_fired(struct player *shooter) {

  struct ltd_stats *ltd = ltd_stat_ptr(shooter);

  ltd->weapons.plasma.fired++;

}

/* plasma hit player */
void ltd_update_plasma_hit(struct player *shooter) {

  struct ltd_stats *ltd = ltd_stat_ptr(shooter);

  ltd->weapons.plasma.hit++;

}

/* plasma phasered */
void ltd_update_plasma_phasered(struct player *shooter) {

  struct ltd_stats *ltd = ltd_stat_ptr(shooter);

  ltd->weapons.plasma.phasered++;

}

/* plasma hit wall */
void ltd_update_plasma_wall(struct player *shooter) {

  struct ltd_stats *ltd = ltd_stat_ptr(shooter);

  ltd->weapons.plasma.wall++;

}

/* plasma damage */
void ltd_update_plasma_damage(struct player *shooter, struct player *victim,
                              const int damage) {

  struct ltd_stats *shooter_ltd = ltd_stat_ptr(shooter);
  struct ltd_stats *victim_ltd = ltd_stat_ptr(victim);

  shooter_ltd->weapons.plasma.damage.inflicted += damage;
  victim_ltd->weapons.plasma.damage.taken += damage;

}

#endif	/* LTD_INLINE */

/*************************************************************************
 * non-inlined LTD stat access functions
 */
                                                                         

/* full reset of all stats */
void ltd_reset(struct player *p) {
  ltd_reset_struct(p->p_stats.ltd);
  ltd_reset_hist(p);
}

/* reset ltd structure */
void ltd_reset_struct(struct ltd_stats (*ltd)[LTD_NUM_SHIPS]) {
  int race, zone, ship;

  memset(ltd, 0, sizeof(struct ltd_stats) * LTD_NUM_SHIPS * LTD_NUM_RACES);

  for (race=0; race<LTD_NUM_RACES; race++) {

    for (ship=0; ship<LTD_NUM_SHIPS; ship++) {

      /* set to 1 so that division by 0 doesn't occur */

      ltd[race][ship].ticks.total = 1;
      ltd[race][ship].ticks.yellow = 1;
      ltd[race][ship].ticks.red = 1;

      for (zone=0; zone<LTD_NUM_TZONES; zone++) {
        ltd[race][ship].ticks.zone[zone] = 1;
      }

      ltd[race][ship].ticks.potential = 1;
      ltd[race][ship].ticks.carrier = 1;
      ltd[race][ship].ticks.repair = 1;
    }

  }

}

/* reset ltd history structure */
void ltd_reset_hist(struct player *p) {

  memset(&(p->p_hist), 0, sizeof(struct ltd_history));

  p->p_hist.num_kills = 0;
  p->p_hist.kill_potential = 0;
  p->p_hist.last_bombed_tick = 1;
  p->p_hist.last_bombed_planet = -2;
  p->p_hist.last_beamup_planet = -2;
  p->p_hist.last_beamdown_planet = -2;
  p->p_hist.enemy_team = 0;

}
  

/* does this player have the necessary stats to advance a rank */
int ltd_can_rank(struct player *p) {

  /* This is the old-style COMPAT mode ranking system */

  float offense_rating, bombing_rating, planet_rating;
  float total_rating, total_hours, total_di, required_di;
  int rank, nextrank;

  offense_rating = ltd_offense_rating(p);
  bombing_rating = ltd_bombing_rating(p);
  planet_rating = ltd_planet_rating(p);
  total_rating = offense_rating + bombing_rating + planet_rating;

  total_hours = (float) ltd_ticks(p, LTD_TOTAL) / 36000.0f;

  total_di = total_rating * total_hours;

  required_di = ranks[p->p_stats.st_rank + 1].hours / hourratio *
                ranks[p->p_stats.st_rank + 1].ratings;

  nextrank = p->p_stats.st_rank + 1;
  if (nextrank >= NUMRANKS)
    return 0;

  /* The person is promoted if they have enough hours and their
   * ratings are high enough, or if it is clear that they could sit
   * around and do nothing for the amount of time required, and still
   * make the rank.  (i.e. their 'DI' rating is about rank.hours
   * rank.ratings ad they have insufficient time yet).
   **/

  rank = p->p_stats.st_rank + 1;

  if (((offense_rating >= ranks[nextrank].offense) || !offense_rank) &&
      ((total_hours > ranks[rank].hours / hourratio &&
        total_rating >= ranks[rank].ratings) ||
       (total_hours < ranks[rank].hours / hourratio &&
        total_di >= required_di)))

    return 1;


  /* We also promote if they belong in their current rank, but have
   * twice enough DI for the next rank.  */

  rank = p->p_stats.st_rank;

  if (((offense_rating >= ranks[nextrank].offense || !offense_rank) &&
       total_rating >= ranks[rank].ratings) &&
      total_di >= required_di * 2)

    return 1;


  /* We also promote if they belong down a rank, but have four times
   * the DI for the next rank.  */

  rank = p->p_stats.st_rank - 1;

  if (p->p_stats.st_rank > 0 &&
      ((offense_rating >= ranks[nextrank].offense || !offense_rank) &&
       total_rating >= ranks[rank].ratings) &&
      total_di >= required_di * 4)

    return 1;


  /* We also promote if they belong down a rank, but have eight times
   * the DI for the next rank, and at least the rank of Captain.  */

  rank = p->p_stats.st_rank - 2;

  if (p->p_stats.st_rank >= 4 &&
      ((offense_rating >= ranks[nextrank].offense || !offense_rank) &&
       total_rating >= ranks[rank].ratings) &&
      total_di >= required_di * 8)

    return 1;

  /* No promotion at this point. */

  return 0;

}

/* Rather than to update the LTD_TOTAL stats independently during each
   tick or event, we call ltd_update_totals to calculate the totals
   each time the player dies. */

#define TOTAL(STAT) \
{ \
  int i, t; \
  for (t=0, i=1; i<LTD_NUM_SHIPS; i++) { \
    if (i == LTD_SB) continue; \
    t += p->p_stats.ltd[race][i].STAT; \
  } \
  p->p_stats.ltd[race][LTD_TOTAL].STAT = t; \
}

#define MAXIMISE(STAT) \
{ \
  int i, m; \
  for (m=0, i=1; i<LTD_NUM_SHIPS; i++) { \
    if (i == LTD_SB) continue; \
    if (m < p->p_stats.ltd[race][i].STAT) m = p->p_stats.ltd[race][i].STAT; \
  } \
  p->p_stats.ltd[race][LTD_TOTAL].STAT = m; \
}

/* update LTD_TOTAL stats */
void ltd_update_totals(struct player *p) {
  int race = ltd_race(p->p_team);

    TOTAL(kills.total);
    MAXIMISE(kills.max);
    TOTAL(kills.first);
    TOTAL(kills.first_potential);
    TOTAL(kills.first_converted);
    TOTAL(kills.second);
    TOTAL(kills.second_potential);
    TOTAL(kills.second_converted);
    TOTAL(kills.phasered);
    TOTAL(kills.torped);
    TOTAL(kills.plasmaed);

    TOTAL(deaths.total);
    TOTAL(deaths.potential);
    TOTAL(deaths.converted);
    TOTAL(deaths.dooshed);
    TOTAL(deaths.phasered);
    TOTAL(deaths.torped);
    TOTAL(deaths.plasmaed);
    TOTAL(deaths.acc);

    TOTAL(planets.taken);
    TOTAL(planets.destroyed);

    TOTAL(bomb.planets);
    TOTAL(bomb.planets_8);
    TOTAL(bomb.planets_core);
    TOTAL(bomb.armies);
    TOTAL(bomb.armies_8);
    TOTAL(bomb.armies_core);

    TOTAL(ogged.armies);
    TOTAL(ogged.dooshed);
    TOTAL(ogged.converted);
    TOTAL(ogged.potential);
    TOTAL(ogged.bigger_ship);
    TOTAL(ogged.same_ship);
    TOTAL(ogged.smaller_ship);
    TOTAL(ogged.sb_armies);
    TOTAL(ogged.friendly);
    TOTAL(ogged.friendly_armies);

    TOTAL(armies.total);
    TOTAL(armies.attack);
    TOTAL(armies.reinforce);
    TOTAL(armies.ferries);
    TOTAL(armies.killed);

    TOTAL(carries.total);
    TOTAL(carries.partial);
    TOTAL(carries.completed);
    TOTAL(carries.attack);
    TOTAL(carries.reinforce);
    TOTAL(carries.ferries);

    TOTAL(ticks.total);
    TOTAL(ticks.yellow);
    TOTAL(ticks.red);
    TOTAL(ticks.zone[0]);
    TOTAL(ticks.zone[1]);
    TOTAL(ticks.zone[2]);
    TOTAL(ticks.zone[3]);
    TOTAL(ticks.zone[4]);
    TOTAL(ticks.zone[5]);
    TOTAL(ticks.zone[6]);
    TOTAL(ticks.zone[7]);
    TOTAL(ticks.potential);
    TOTAL(ticks.carrier);
    TOTAL(ticks.repair);

    TOTAL(damage_repaired);

    TOTAL(weapons.phaser.fired);
    TOTAL(weapons.phaser.hit);
    TOTAL(weapons.phaser.damage.inflicted);
    TOTAL(weapons.phaser.damage.taken);

    TOTAL(weapons.torps.fired);
    TOTAL(weapons.torps.hit);
    TOTAL(weapons.torps.detted);
    TOTAL(weapons.torps.selfdetted);
    TOTAL(weapons.torps.wall);
    TOTAL(weapons.torps.damage.inflicted);
    TOTAL(weapons.torps.damage.taken);

    TOTAL(weapons.plasma.fired);
    TOTAL(weapons.plasma.hit);
    TOTAL(weapons.plasma.phasered);
    TOTAL(weapons.plasma.wall);
    TOTAL(weapons.plasma.damage.inflicted);
    TOTAL(weapons.plasma.damage.taken);
}

#endif /* LTD_STATS */

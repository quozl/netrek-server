/*
 * orbit.c
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "util.h"

#define planet_(i)       (planets + (i))
#define player_(i)       (players + (i))

#define firstPlayer      (player_(0))
#define lastPlayer       (player_(MAXPLAYER - 1))

#define firstPlanet      (planet_(0))
#define lastPlanet       (planet_(MAXPLANETS - 1))

#define isAlive(p)              ((p)->p_status == PALIVE)

void de_lock(struct player *me)
{
  me->p_flags   &= ~(PFPLOCK | PFPLLOCK | PFTRACT | PFPRESS);
  /* upon arrival at a base, during a dock attempt, if the dock fails,
  and we were transwarping in, force a refit delay for five
  seconds. */
  if (me->p_flags & PFTWARP){
    me->p_flags &= ~PFTWARP;
    me->p_flags |= PFREFITTING;
    rdelay       = me->p_updates + 50;
  }
}

/*
 * Dock with a ship;
 * assumes it is a base, is alive, and is friendly.
 * (If one or more of these is not true, various nasty things happen.)
 */ 

#define DOCK_NO_BASE 0 /* no base is close enough */
#define DOCK_FAILURE 1 /* dock failed due to some condition */
#define DOCK_SUCCESS 2 /* dock was successful */

static int dock(struct player *base)
{
  LONG dx, dy;
  int bay_no;
  u_char dir_from_base;

  /*
   * See if I am close enough:  */
  dx = base->p_x - me->p_x;
  if ((dx > DOCKDIST) || (dx < -DOCKDIST))
    return DOCK_NO_BASE;
  dy = base->p_y - me->p_y;
  if ((dy > DOCKDIST) || (dy < -DOCKDIST))
    return DOCK_NO_BASE;
  if (dx*dx + dy*dy > DOCKDIST*DOCKDIST)
    return DOCK_NO_BASE;

  /*
   * Can't dock on enemy bases:  */
  if (!friendlyPlayer(base)) {
    new_warning(UNDEF, "Docking onto a hostile base is unwise, Captain!");
    de_lock(me);
    return DOCK_FAILURE;
  }

  /*
   * Can't dock if you aren't allowed to */
  if (me->p_candock == 0) {
    if (send_short)
      swarning(SBDOCKREFUSE_TEXT, base->p_no, 0);
    else
      new_warning(UNDEF, 
		  "Starbase %s refusing us docking permission, Captain.",
		  base->p_name);
    de_lock(me);
    return DOCK_FAILURE;
  }

  /* Disallow SB to SB docking */
  if (me->p_ship.s_type == STARBASE && !chaosmode) {
    new_warning(UNDEF, "Starbases are too big to dock onto other starbases.");
    de_lock(me);
    return DOCK_FAILURE;
  }

  /*
   * See if the base is allowing docking: */
  if (! (base->p_flags & PFDOCKOK)) {
    new_warning(UNDEF, 
		"Starbase %s refusing all docking requests, Captain.",
		base->p_name);
    de_lock(me);
    return DOCK_FAILURE;
  }
  
  /*
   * Make sure the base's docking bays are not already full: */
  bay_no = bay_closest(base, dx, dy);
  if (bay_no == -1) {
    if (send_short)
      swarning(SBDOCKDENIED_TEXT, base->p_no, 0);
    else
      new_warning(UNDEF, "Starbase %s: Permission to dock denied, all bays currently occupied.", base->p_name);
    de_lock(me);
    return DOCK_FAILURE;
  }

  /*
   * Adjust player structures of myself and the base. */
  dir_from_base   = ((bay_no * 90 + 45) * 256) / 360;
  me->p_flags    &= ~(PFPLOCK | PFPLLOCK | PFTRACT | PFPRESS);
  me->p_flags    |= PFDOCK;
  me->p_dir       = 64 + dir_from_base;
  me->p_desdir    = me->p_dir;
  me->p_x         = base->p_x + DOCKDIST * Cos[dir_from_base];
  me->p_y         = base->p_y + DOCKDIST * Sin[dir_from_base];
  p_x_y_to_internal(me);
  me->p_speed     = 0;
  me->p_desspeed  = 0;
  /* Should never happen, as PFTWARP is unset once player reaches
     2*DOCKDIST from base during auto_features() routine */
  if (me->p_flags & PFTWARP){
    me->p_flags &= ~PFTWARP;
    me->p_flags |= PFREFITTING;
    rdelay       = me->p_updates + 50;
  }

  bay_claim(base, me, bay_no);

  /*
   * Notify player of success. */
  if (send_short)
    swarning(ONEARG_TEXT, 2, bay_no);
  else
    new_warning(UNDEF,"Helmsman:  Docking manuever completed Captain.  All moorings secured at bay %d.", bay_no);
  return DOCK_SUCCESS;
}


/*
 * Process orbit request.
 */
void orbit(void)
{
  struct planet *l;
  struct player *j, *tractoring_player;
  u_char dir;
  LONG dx, dy;

  if (me->p_inl_draft != INL_DRAFT_OFF) return;

  /*
   * Make sure ship is going slowly enough to orbit: */
  if (me->p_speed > ORBSPEED) {
    new_warning(79, "Helmsman: Captain, the maximum safe speed for docking or orbiting is warp 2!");
    return;
  }

  /* 
   * See who has us tractored/pressored: */
  tractoring_player = NULL;
  for (j = firstPlayer; j <= lastPlayer; j++) {
    if (isAlive(j) && (j->p_flags & PFTRACT) && (j->p_tractor == me->p_no)) {
      /*
       * Orbitting is never possible with more than one T/P beam on us,
       * or with even one beam on except from a base: */
      if ((j->p_ship.s_type != STARBASE) || (tractoring_player != NULL))
	return;
      me->p_flags &= ~(PFDOCK | PFORBIT);
      tractoring_player = j;
    }
  }

  /*
   * If exactly one tractoring player, check for docking there, 
   * then return since no other orbiting or docking is allowed.   */
  if (tractoring_player != NULL) {
    dock(tractoring_player);
    return;
  }

  /*
   * If already docked or orbiting, nothing more needs to be done: */
  if (me->p_flags & (PFDOCK | PFORBIT))
    return;

  /*
   * Try to dock on all live starbases: */
  if (me->p_ship.s_type != STARBASE || chaosmode) {
    for (j = firstPlayer; j <= lastPlayer; j++) {
      if (j == me)
	continue;
      if (j->p_ship.s_type != STARBASE)
	continue;
      if (! isAlive(j))
	continue;
      switch (dock(j)) {
      case DOCK_NO_BASE:
	break;
      case DOCK_FAILURE:
	return;
      case DOCK_SUCCESS:
	return;
      }
    }
  }

  /*
   * No starbase in range... try for a planet: */
  for (l = firstPlanet; l <= lastPlanet; l++) {
    dx = l->pl_x - me->p_x;
    if ((dx > ENTORBDIST) || (dx < -ENTORBDIST))
      continue;
    dy = l->pl_y - me->p_y;
    if ((dy > ENTORBDIST) || (dy < -ENTORBDIST))
      continue;
    if (dx*dx + dy*dy > ENTORBDIST*ENTORBDIST)
      continue;

    if (!sborbit) {
      if ((me->p_ship.s_type == STARBASE) && (!(me->p_team & l->pl_owner))) {
        new_warning(80, "Central Command regulations prohibit you from orbiting foreign planets");
        return;
      }
    }

    /*
     * A successful orbit!
     * Adjust me and the planet orbited to reflect the new orbit situation.
     * In particular, my direction and x,y position probably change.  */
    dir = (u_char) rint(atan2((double) (me->p_x - l->pl_x),
			      (double) (l->pl_y - me->p_y)) / M_PI * 128.);
    /*
     * See if this is the first touch of this planet: */
    if ((l->pl_info & me->p_team) == 0) {
#ifndef LTD_STATS
#ifdef FLAT_BONUS                        
      /* Give 1 army credit */
      if ((l->pl_armies < 10) && (status->tourn) &&
	  (realNumShips(l->pl_owner) >=3)) {
	me->p_stats.st_tarmsbomb++;
	status->armsbomb++;
      } 
#endif
#endif
      l->pl_info |= me->p_team;
    }
    me->p_flags &= ~(PFPLOCK | PFPLLOCK | PFTRACT | PFPRESS);
    me->p_flags |= PFORBIT;
    me->p_dir    = dir + 64;
    me->p_desdir = dir + 64;
    p_x_y_go(me, l->pl_x + ORBDIST * Cos[dir], l->pl_y + ORBDIST * Sin[dir]);
    me->p_speed  = me->p_desspeed = 0;
    me->p_planet = l->pl_no;
    return;
  }
  
  new_warning(81, "Helmsman:  Sensors read no valid targets in range to dock or orbit sir!");
}

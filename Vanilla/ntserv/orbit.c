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

#define planet_(i)       (planets + (i))
#define player_(i)       (players + (i))

#define firstPlayer      (player_(0))
#define lastPlayer       (player_(MAXPLAYER - 1))

#define firstPlanet      (planet_(0))
#define lastPlanet       (planet_(MAXPLANETS - 1))

#define isAlive(p)              ((p)->p_status == PALIVE)

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/*
 * If there are bugs in this docking code and/or possibly other places
 * in the code, then conceivably a base my have p_docked < 4 while
 * all ports are non-VACANT.  That is inconsistent, and this code
 * will check for that eventuality on the fly.
 */
#if 1
#define RETURN_IF_VACANT(base, port)
#else
#define RETURN_IF_VACANT(base, port) \
        if (base->p_port[port] != VACANT) { \
	  ERROR(1, ("Starbase %d has no ports vacant but p_docked==%d\n", \
		    base->p_id, base->p_docked)); \
	  return FALSE; \
	}
#endif

void de_lock(struct player *me)
{
  me->p_flags   &= ~(PFPLOCK | PFPLLOCK | PFTRACT | PFPRESS);
 #ifdef SB_TRANSWARP
  if (me->p_flags & PFTWARP){
    me->p_flags &= ~PFTWARP;
    me->p_flags |= PFREFITTING;
    rdelay       = me->p_updates + 50;
  }
#endif
}

/*
 * Dock with a ship;
 * assumes it is a base, is alive, and is friendly.
 * (If one or more of these is not true, various nasty things happen.)
 */ 

#define DOCK_NO_BASE 0
#define DOCK_FAILURE 1
#define DOCK_SUCCESS 2

static int dock(struct player *base)
{
  LONG dx, dy;
  int port_id;
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
  if (me->p_ship.s_type == STARBASE) {
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
   * Make sure the base's docking ports are not already full: */
  if (base->p_docked >= NUMPORTS) {
    if (send_short)
      swarning(SBDOCKDENIED_TEXT, base->p_no, 0);
    else
      new_warning(UNDEF, "Starbase %s: Permission to dock denied, all ports currently occupied.", base->p_name);
    de_lock(me);
    return DOCK_FAILURE;
  }

  /*
   * Dock on closest port.
   * A starbase's ports are     3    0
   * numbered as in this          ()
   * picture:                   2    1                       */
  if (dx > 0) {
    /* We are to the left of the base: */
    if (dy > 0) {
      /* Above and to left of base: */
      if (base->p_port[3] == VACANT)
	port_id = 3;
      else if (base->p_port[2] == VACANT)
	port_id = 2;
      else if (base->p_port[0] == VACANT) 
	port_id = 0;
      else {
	RETURN_IF_VACANT(base, 1);
	port_id = 1;
      }
    } else {
      /* Below and to left of base: */
      if (base->p_port[2] == VACANT)
	port_id = 2;
      else if (base->p_port[3] == VACANT)
	port_id = 3;
      else if (base->p_port[1] == VACANT) 
	port_id = 1;
      else {
	RETURN_IF_VACANT(base, 0);
	port_id = 0;
      }
    }
  } else {
    /* We are to the right of the base: */
    if (dy > 0) {
      /* Above and to right of base: */
      if (base->p_port[0] == VACANT)
	port_id = 0;
      else if (base->p_port[1] == VACANT)
	port_id = 1;
      else if (base->p_port[3] == VACANT) 
	port_id = 3;
      else {
	RETURN_IF_VACANT(base, 2);
	port_id = 2;
      }
    } else {
      /* Below and to right of base: */
      if (base->p_port[1] == VACANT)
	port_id = 1;
      else if (base->p_port[2] == VACANT)
	port_id = 2;
      else if (base->p_port[0] == VACANT) 
	port_id = 0;
      else {
	RETURN_IF_VACANT(base, 3);
	port_id = 3;
      }
    }
  }

  /*
   * Adjust player structures of myself and the base. */
  dir_from_base = ((port_id * 90 + 45) * 256) / 360;
  me->p_flags   &= ~(PFPLOCK | PFPLLOCK | PFTRACT | PFPRESS);
  me->p_flags   |= PFDOCK;
  me->p_dir      = 64 + dir_from_base;
  me->p_desdir   = me->p_dir;
  me->p_x        = base->p_x + DOCKDIST * Cos[dir_from_base];
  me->p_y        = base->p_y + DOCKDIST * Sin[dir_from_base];
  me->p_speed    = 0;
  me->p_desspeed = 0;
  me->p_docked   = base->p_no;
  me->p_port[0]  = port_id;
#ifdef SB_TRANSWARP
  if (me->p_flags & PFTWARP){
    me->p_flags &= ~PFTWARP;
    me->p_flags |= PFREFITTING;
    rdelay       = me->p_updates + 50;
  }
#endif

  base->p_docked++;
  base->p_port[port_id] = me->p_no;

  /*
   * Notify player of success. */
  if (send_short)
    swarning(ONEARG_TEXT, 2, port_id);
  else
    new_warning(UNDEF,"Helmsman:  Docking manuever completed Captain.  All moorings secured at port %d.", port_id);
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
   * then return since no other orbitting or docking is allowed.   */
  if (tractoring_player != NULL) {
    dock(tractoring_player);
    return;
  }

  /*
   * If already docked or orbitting, nothing more needs to be done: */
  if (me->p_flags & (PFDOCK | PFORBIT))
    return;

  /*
   * Try to dock on all live starbases: */
  if (me->p_ship.s_type != STARBASE) {
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

    /*
     * Bases cannot orbit non-owned planets except in BASE WARS! mode.  */
#ifndef BASE_WARS
    if ((me->p_ship.s_type == STARBASE) && (!(me->p_team & l->pl_owner))) {
      new_warning(80, "Central Command regulations prohibits you from orbiting foreign planets");
      return;
    }
#endif

    /*
     * A successful orbit!
     * Adjust me and the planet orbitted to reflect the new orbit situation.
     * In particular, my direction and x,y position probably change.  */
    dir = (u_char) nint(atan2((double) (me->p_x - l->pl_x),
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
    me->p_x      = l->pl_x + ORBDIST * Cos[dir];
    me->p_y      = l->pl_y + ORBDIST * Sin[dir];
    me->p_speed  = me->p_desspeed = 0;
    me->p_planet = l->pl_no;
    return;
  }
  
  new_warning(81, "Helmsman:  Sensors read no valid targets in range to dock or orbit sir!");
}

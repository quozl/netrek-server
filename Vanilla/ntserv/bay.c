#include "copyright.h"
#include "config.h"
#include <assert.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

/* starbase bay functions */

/* convert a player number to a player struct pointer */
static struct player *p_no(int i)
{
  return &players[i];
}

/* convert a docked ship player to the base they are docked to */
struct player *bay_owner(struct player *me)
{
  assert(me->p_flags & PFDOCK);
  return p_no(me->p_dock_with);
}

/* check all bays for consistency, despite our best efforts it is
still possible for the bays to be inconsistent, an example is a
SIGKILL of ntserv */
void bay_consistency_check(struct player *base)
{
  int i;
  struct player *ship;

  for (i=0; i<NUMBAYS; i++) {
    if (base->p_bays[i] == VACANT) continue;
    ship = p_no(base->p_bays[i]);
    if (!(ship->p_flags & PFDOCK)) {
      /* ship isn't docked */
      base->p_bays[i] = VACANT;
      continue;
    }
    if (ship->p_dock_with != base->p_no) {
      /* ship is docked but not with this base */
      base->p_bays[i] = VACANT;
      continue;
    }
    if (ship->p_dock_bay != i) {
      /* ship is docked with this base but not on this bay */
      base->p_bays[i] = VACANT;
      continue;
    }
  }
}

/* claim a specific bay on a base for use by a docking ship */
void bay_claim(struct player *base, struct player *me, int bay_no)
{
  me->p_dock_with = base->p_no;
  me->p_dock_bay  = bay_no;
  base->p_bays[bay_no] = me->p_no;
}

/* release of a bay by a docked ship */
void bay_release(struct player *me)
{
  if (me->p_flags & PFDOCK) {
    struct player *base = p_no(me->p_dock_with);
    int bay = me->p_dock_bay;
    if (base->p_bays[bay] != VACANT) {
      base->p_bays[bay] = VACANT;
    }
    me->p_flags &= ~PFDOCK;
  }
}

/* release of all bays by a base */
void bay_release_all(struct player *base)
{
  int i;
  for (i=0; i<NUMBAYS; i++) 
    if (base->p_bays[i] != VACANT) {
      struct player *pl = p_no(base->p_bays[i]);
      base->p_bays[i] = VACANT;
      pl->p_flags &= ~PFDOCK;
    }
}

/* initialise all bays */
void bay_init(struct player *me)
{
  int i;
  for(i=0;i<NUMBAYS;i++)
    me->p_bays[i] = VACANT;
}

/*
 * Return closest bay according to position.
 * A starbase's bays are      3    0
 * numbered as in this          ()
 * picture:                   2    1                       */
int bay_closest(struct player *base, LONG dx, LONG dy)
{
  bay_consistency_check(base);
  if (dx > 0) {
    /* We are to the left of the base: */
    if (dy > 0) {
      /* Above and to left of base: */
      if (base->p_bays[3] == VACANT) return 3;
      else if (base->p_bays[2] == VACANT) return 2;
      else if (base->p_bays[0] == VACANT) return 0;
      else if (base->p_bays[1] == VACANT) return 1;
      else return -1;
    } else {
      /* Below and to left of base: */
      if (base->p_bays[2] == VACANT) return 2;
      else if (base->p_bays[3] == VACANT) return 3;
      else if (base->p_bays[1] == VACANT) return 1;
      else if (base->p_bays[0] == VACANT) return 0;
      else return -1;
    }
  } else {
    /* We are to the right of the base: */
    if (dy > 0) {
      /* Above and to right of base: */
      if (base->p_bays[0] == VACANT) return 0;
      else if (base->p_bays[1] == VACANT) return 1;
      else if (base->p_bays[3] == VACANT) return 3;
      else if (base->p_bays[2] == VACANT) return 2;
      else return -1;
    } else {
      /* Below and to right of base: */
      if (base->p_bays[1] == VACANT) return 1;
      else if (base->p_bays[2] == VACANT) return 2; /* inconsistent order */
      else if (base->p_bays[0] == VACANT) return 0;
      else if (base->p_bays[3] == VACANT) return 3;
      else return -1;
    }
  }
}

/*! @file bay.c
    Starbase bay manipulation.
    Operations on the docking bays of a starbase.
    */

#include "copyright.h"
#include "config.h"
#include <assert.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "util.h"
#include "shmatomic.h"

/*! @brief Identify the base a ship is docked to.
    @warning Checks player::p_flags. Must only be called if a ship is docked.
    Generates an assertion failure if not.
    @param me player pointer of docked ship.
    @return player pointer of base.
    */
struct player *bay_owner(struct player *me)
{
  assert(me->p_flags & PFDOCK);
  return p_no(me->p_dock_with);
}

/*! @brief Check bays for consistency.
    @details Iterates through the bays of a base to ensure that each
    one is either VACANT or points to a ship that is docked to the
    base.  Despite our best efforts it is still possible for the bays
    to be inconsistent, an example is a SIGKILL of ntserv.
    @param base player pointer of base.
    @return none.
    */
void bay_consistency_check(struct player *base)
{
  int i;
  struct player *ship;

  for (i=0; i<NUMBAYS; i++) {
    int pno = load_acquire(&base->p_bays[i]);
    if (pno == VACANT) continue;
    if (pno < 0 || pno >= MAXPLAYER) {
      compare_and_swap(&base->p_bays[i], pno, VACANT);
      continue;
    }
    ship = p_no(pno);
    if (!(ship->p_flags & PFDOCK)) {
      /* ship isn't docked */
      compare_and_swap(&base->p_bays[i], pno, VACANT);
      continue;
    }
    if (ship->p_dock_with != base->p_no) {
      /* ship is docked but not with this base */
      compare_and_swap(&base->p_bays[i], pno, VACANT);
      continue;
    }
    if (ship->p_dock_bay != i) {
      /* ship is docked with this base but not on this bay */
      compare_and_swap(&base->p_bays[i], pno, VACANT);
      continue;
    }
  }
}

/*! @brief Claim a specific bay on a base for docking.
    @param base player pointer of base.
    @param me player pointer of docking ship.
    @param bay_no which bay to claim.
    @return none.
    */
void bay_claim(struct player *base, struct player *me, int bay_no)
{
  me->p_dock_with = base->p_no;
  me->p_dock_bay  = bay_no;
  store_release(&base->p_bays[bay_no], me->p_no);
}

/*! @brief Release of a bay by a docked ship.
    @param me player pointer of potentially docked ship.
    @return none.
    */
void bay_release(struct player *me)
{
  if (me->p_flags & PFDOCK) {
    int dock_with = me->p_dock_with;
    int bay = me->p_dock_bay;
    atomic_and(&me->p_flags, ~PFDOCK);
    if (dock_with >= 0 && dock_with < MAXPLAYER &&
        bay >= 0 && bay < NUMBAYS) {
      struct player *base = p_no(dock_with);
      compare_and_swap(&base->p_bays[bay], me->p_no, VACANT);
    }
  }
}

/*! @brief Release of all bays by a base.
    @param base player pointer of base.
    @return none.
    */
void bay_release_all(struct player *base)
{
  int i;
  for (i=0; i<NUMBAYS; i++) {
    int pno = atomic_exchange(&base->p_bays[i], VACANT);
    if (pno >= 0 && pno < MAXPLAYER) {
      struct player *pl = p_no(pno);
      atomic_and(&pl->p_flags, ~PFDOCK);
    }
  }
}

/*! @brief Initialise all bays.
    @param me player pointer of ship.
    @return none.
    */
void bay_init(struct player *me)
{
  int i;
  for(i=0;i<NUMBAYS;i++)
    me->p_bays[i] = VACANT;
}

/*! @brief Return closest bay to a position.
    @details Bays are numbered clockwise from top right (0), bottom
    right (1), bottom left (2), and top left (3).  See source for
    ASCII art map of bays.
    @param base player pointer of base.
    @return bay_no closest bay number of base.
    */
/*
 * A starbase's bays are      3    0
 * numbered as in this          ()
 * picture:                   2    1
*/
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

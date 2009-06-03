/*
 * detonate.c
 */
#include "copyright.h"

#include <stdio.h>
#include <sys/types.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

#define Teammates(a, b) \
            (((a)->p_team == (b)->p_team) && !((a)->p_flags & PFPRACTR))


void detothers(void)
{
  int dx, dy;
  struct player *j;
  struct torp *t;

  if (status->gameup & GU_CONQUER) return;

  if (me->p_fuel < myship->s_detcost) {
    new_warning(UNDEF, "Not enough fuel to detonate");
    return;
  }
  if (me->p_flags & PFWEP) {
    new_warning(UNDEF, "Weapons overheated");
    if (!chaosmode)
      return;
  }

  me->p_fuel  -= myship->s_detcost;
  me->p_wtemp += myship->s_detcost / 5;

  for (j = firstPlayer; j <= lastPlayer; j++) {
    if (j->p_status == PFREE)
      continue;
    /*
     * Teammates' torps are not always friendly--in dogfight mode, 
     * combatants are on the same team!  Commenting this out.  -tom 
    if (Teammates(j, me))
      continue;  */
    /*
     * Not a teammate... must look at each individual torp for its status. */
    for (t = firstTorpOf(j); t <= lastTorpOf(j); t++) {
      /*
       * Can't det peaceful torps. */
      if (friendlyTorp(t))
	continue;
      if (t->t_status == TMOVE) {
	dx = t->t_x - me->p_x;
	if ((dx > DETDIST) || (dx < -DETDIST))
	  continue;
	dy = t->t_y - me->p_y;
	if ((dy > DETDIST) || (dy < -DETDIST))
	  continue;
	if (dx*dx + dy*dy <= DETDIST*DETDIST) {
	  t->t_status = TDET;
	  t->t_whodet = me->p_no;
#ifdef LTD_STATS
          /* torp was detted, update stats */
          ltd_update_torp_detted(j);
#endif
	}
      }
    }
  }
}

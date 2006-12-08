#include <stdio.h>
#include "copyright.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "sturgeon.h"

#ifdef STURGEON

static void sturgeon_weapon_switch()
{
  char buf[80];
  int t = me->p_special;

  if (t < 0) {
    new_warning(UNDEF, "This ship is not equipped with special weapons.");
    return;
  }

  for (t = (t+1) % NUMSPECIAL; me->p_weapons[t].sw_number == 0 &&
	 t != me->p_special; t = (t+1) % NUMSPECIAL)
    ;
  
  if ((t == me->p_special) && (me->p_weapons[t].sw_number == 0)) {
    new_warning(UNDEF, "We have exhausted all of our special weapons, Captain.");
    me->p_special = -1;
  } else {
    if (me->p_weapons[t].sw_number < 0)  /* infinite */
      sprintf(buf, "Special Weapon: %s", me->p_weapons[t].sw_name);
    else
      sprintf(buf, "Special Weapon: %s (%d left)",
	      me->p_weapons[t].sw_name, me->p_weapons[t].sw_number);
    
    new_warning(UNDEF,buf);
    me->p_special = t;
  }
}

int sturgeon_coup()
{
  struct planet *l;

  /* Coup key (rarely used) doubles as toggle key to switch between various
     special weapons in inventory */
  l = &planets[me->p_planet];
  /* Make sure player not actually in a position to coup */
  if ((!(me->p_flags & PFORBIT) ||
       !(l->pl_flags & PLHOME) ||
       (l->pl_owner == me->p_team) ||
       !(l->pl_flags & me->p_team))) {
    sturgeon_weapon_switch();
    return 0;
  }
  return 1;
}
#endif

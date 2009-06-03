/* plasma.c -- Create a new plasma with a given direction and attributes
 */
#include "copyright.h"

#include <stdio.h>
#include <sys/types.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "sturgeon.h"
#include "util.h"

/*
 * See torp.c for comments; in essence this code is an exact copy of that
 * code with a few cosmetic changes.  Which indicates we should probably
 * parameterize torps further and be done with it, rather like ship types
 * are now.  However, that is for the future; right now two types of torps
 * are all we have, and thus code is only duplicated.
 */

void nplasmatorp(u_char course, int attributes)
{
  struct torp *k;

#ifdef STURGEON
  if (sturgeon && sturgeon_special_weapons) {
    sturgeon_nplasmatorp(course, attributes);
    return;
  }
#endif

  if (weaponsallowed[WP_PLASMA]==0) {
    new_warning(17,"Plasmas haven't been invented yet.");
    return;
  }
  if (me->p_ship.s_plasmacost < 0) {
    new_warning(18,"Weapon's Officer:  Captain, this ship is not armed with plasma torpedoes!");
    return;
  }
  if (me->p_flags & PFWEP) {
    new_warning(19,"Plasma torpedo launch tube has exceeded the maximum safe temperature!");
    if (!chaosmode)
      return;
  }
  if (me->p_nplasmatorp == MAXPLASMA) {
    if (me->p_ship.s_type != ATT) {
      new_warning(20,"Our fire control system limits us to 1 live torpedo at a time captain!");
      return;
    } 
  }
  if (me->p_fuel < myship->s_plasmacost) {
    new_warning(22,"We don't have enough fuel to fire a plasma torpedo!");
    return;
  }
  if (me->p_flags & PFREPAIR) {
    new_warning(23,"We cannot fire while our vessel is undergoing repairs.");
    return;
  }
  if ((me->p_cloakphase) && (me->p_ship.s_type != ATT)) {
    new_warning(24,"We are unable to fire while in cloak, captain!");
    return;
  }
  for (k = firstPlasmaOf(me); k <= lastPlasmaOf(me); k++)
    if (k->t_status == TFREE)
      break;
  if (k > lastPlasmaOf(me))  {
    new_warning(UNDEF,"We are out of plasma, for the moment, Captain!");
    return;
  }
#if 0
  /* Pseudo code for run-time debugging, if someone wants it. */
  if (k > lastPlasmaOf(me))
    error();
#endif

  me->p_nplasmatorp++;
  me->p_fuel -= myship->s_plasmacost;
  me->p_wtemp += (myship->s_plasmacost / 10) - 8;  /* Heat weapons */
    
  k->t_status = TMOVE;
  k->t_type = TPLASMA;
  k->t_attribute = attributes;
  k->t_owner = me->p_no;
  t_x_y_set(k, me->p_x, me->p_y);
  k->t_turns = myship->s_plasmaturns;
  k->t_damage = myship->s_plasmadamage;
  k->t_gspeed = (attributes & TVECTOR) 
    ? torpGetVectorSpeed(me->p_dir, me->p_speed, course, myship->s_plasmaspeed)
      : myship->s_plasmaspeed * WARP1;
  k->t_fuse   = myship->s_plasmafuse * T_FUSE_SCALE;
  k->t_dir    = ((myship->s_type == STARBASE) || 
                 (myship->s_type == ATT)) ? course : me->p_dir;
  k->t_war    = me->p_war;
  k->t_team   = me->p_team;
  k->t_whodet = NODET;
#if 0
  k->t_speed = k->t_gspeed / WARP1;
#endif

#ifdef LTD_STATS

  /* At this point, a plasma was fired */
  ltd_update_plasma_fired(me);

#endif

}

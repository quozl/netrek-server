/* torp.c -- Create a new torp with a given direction and attributes
 */
#include "copyright.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "util.h"

int torpGetVectorSpeed(u_char pdir, int pspeed, u_char  tdir, int tspeed)
{
  double		vsx, vsy, vtx, vty, ntx, nty;
  int			newspeed;

  /* ship vector */
  vsx = (double) Cos[pdir] * (pspeed * WARP1);
  vsy = (double) Sin[pdir] * (pspeed * WARP1);
  /* torp vector */
  vtx = (double) Cos[tdir] * (tspeed * WARP1);
  vty = (double) Sin[tdir] * (tspeed * WARP1);

  ntx = vsx + vtx;
  nty = vsy + vty;

  newspeed = rint(sqrt(ntx*ntx + nty*nty));
  MATH_ERR_CHECK((int)(ntx*ntx + nty*nty));

#if 0
  printf("vectortorp: ship (s=%d,d=%d,t=%d), result: %d\n", 
	 pspeed, pdir, tspeed, newspeed / WARP1); 
  fflush(stdout);
#endif
  if (newspeed > 20*WARP1)
    newspeed = 20*WARP1;

  return newspeed;
}


/*
 * If a set of given conditions are met, fire a single torp in direction
 * course.  Use different attributes 
 *
 * torp->t_fuse is the life of the torpedo.  It is set here based on
 * a random function.  Torps currently live two to five seconds.
 */
void ntorp(u_char course, int attributes)
{
  static LONG last_torp_fired_update = 0;
  struct torp *k;

  if (status->gameup & GU_CONQUER) return;

  /*
   * Prevent player from firing more than one torp per update.  */
  if (me->p_updates == last_torp_fired_update)
    return;

  if (me->p_flags & PFWEP) {
    new_warning(25, "Torpedo launch tubes have exceeded maximum safe temperature!");
    if (!chaosmode)
      return;
  }
  if (me->p_ntorp == MAXTORP) {
    new_warning(26, "Our computers limit us to having 8 live torpedos at a time captain!");
    return;
  }
  if (me->p_fuel < myship->s_torpcost) {
    new_warning(27, "We don't have enough fuel to fire photon torpedos!");
    return;
  }
  if (me->p_flags & PFREPAIR) {
    new_warning(28, "We cannot fire while our vessel is in repair mode.");
    return;
  }
  if ((me->p_cloakphase) && (me->p_ship.s_type != ATT)) {
#ifdef STURGEON
    if (sturgeon) {
      if (!me->p_upgradelist[UPG_FIRECLOAK]) {
        new_warning(UNDEF,"We don't have the proper upgrade to fire while cloaked, captain!");
        return;
      }
      if (me->p_fuel < myship->s_torpcost * 2) {
        new_warning(UNDEF,"We don't have enough fuel to fire photon torpedos while cloaked!");
        return;
      }
    }
    else
#endif
    {
      new_warning(29, "We are unable to fire while in cloak, captain!");
      return;
    }
  }


  /* 
   * If topgun mode,
   * Permit firing only if out of the front of the ship  */
  if (topgun && ((me->p_ship).s_type != STARBASE)) {
    int delta;
    if ((delta = ((int) me->p_dir - (int) course)) < 0)
      delta = -delta;
    if ((delta > topgun)  && (delta < (256 - topgun))) { 
      /* note: 128 = 180 degrees left/right */
      new_warning(30, "We only have forward mounted cannons.");
      return;
    }
  }


  /*
   * Find a free torp to use */
  for (k = firstTorpOf(me); k <= lastTorpOf(me); k++)
    if (k->t_status == TFREE)
      break;
#if 0
  /* Pseudo code for run-time debugging, if someone wants it. */
  if (k > lastTorpOf(me))
    error();
#endif

  last_torp_fired_update = me->p_updates;

  /*
   * Change my ship state: less fuel, more weapon temp  */
  me->p_ntorp++;
#ifdef STURGEON
  if (sturgeon && me->p_cloakphase)
    me->p_fuel -= myship->s_torpcost * 2;
  else
#endif
  me->p_fuel -= myship->s_torpcost;
  me->p_wtemp += (myship->s_torpcost / 10) - 10;	/* Heat weapons */

  /* 
   * Initialize torp: */
  k->t_status = TMOVE;
  k->t_type = TPHOTON;
  k->t_attribute = attributes;
  k->t_owner = me->p_no;
  t_x_y_set(k, me->p_x, me->p_y);
  k->t_turns  = myship->s_torpturns;
  k->t_damage = myship->s_torpdamage;
  k->t_gspeed = (attributes & TVECTOR) ?
    torpGetVectorSpeed(me->p_dir, me->p_speed, course, myship->s_torpspeed) :
      myship->s_torpspeed * WARP1;
  k->t_fuse   = (myship->s_torpfuse + (random() % 20)) * T_FUSE_SCALE;
  k->t_dir    = course;
  k->t_war    = me->p_war;
  k->t_team   = me->p_team;
  k->t_whodet = NODET;
#if 0
  k->t_no = i;
  k->t_speed = k->t_gspeed / WARP1;
#endif
#ifdef STURGEON
  k->t_spinspeed = 0;
#endif

#ifdef LTD_STATS

  /* At this point, a torpedo was fired */
  ltd_update_torp_fired(me);

#endif

}


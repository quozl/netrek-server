/*
 * phaser.c
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"
#include "ltd_stats.h"
#include "util.h"

void phaser(u_char course)
{
  struct player *j, *target_player = NULL;
  struct torp *t, *target_plasma = NULL;
  int pstatus;
  double s, C,D;
  LONG A,B, E,F;
  LONG dx, dy, myphrange, range;
  U_LONG range_sq, this_range_sq;
  struct phaser *mine = &phasers[me->p_no];

  if (status->gameup & GU_CONQUER) return;

  if (mine->ph_status != PHFREE) {
    new_warning(32, "Phasers have not recharged.");
    return;
  }
  if (me->p_fuel < myship->s_phasercost) {
    new_warning(33, "Not enough fuel for phaser.");
    return;
  }
  if (me->p_flags & PFREPAIR) {
    new_warning(34, "Can't fire while repairing.");
    return;
  }
  if (me->p_flags & PFWEP) {
    new_warning(35, "Weapons overheated");
    if (!chaosmode)
      return;
  }
  /***** The following should really test a ship attribute, not type. */
  if ((me->p_cloakphase) && (me->p_ship.s_type != ATT)) {
    new_warning(36, "Cannot fire while cloaked");
    return;
  }
  if (myship->s_phasercost < 0) {
    new_warning(31, "Weapons Officer:  This ship is not armed with phasers, captain!");
    return;
  }

  /*
   * (C, D) is a point on the my phaser line, relative to me.  It 
   * doesn't matter what point is used, so long as it is reasonably
   * far from me (to prevent round-off error problems).              */
  C = Cos[course]*10*PHASEDIST;
  D = Sin[course]*10*PHASEDIST;

  /*
   * Establish necessary preconditions for for-loops:                */
  myphrange = (PHASEDIST * me->p_ship.s_phaserdamage) / 100;
  range  = myphrange + 1;
  range_sq = myphrange*myphrange + 1;
  pstatus = PHMISS;

  /*
   * See if phaser hit a player: */
  for (j = firstPlayer; j <= lastPlayer; j++) {
    /*
     * Don't check dead players or me:                               */
    if ((j->p_status != PALIVE) || (j == me))
      continue;
    
    /*
     * Make sure we are not mutually at peace:                       */
    if ((!(j->p_war & me->p_team)) && 
	(!(me->p_war & j->p_team)))
      continue;
      
    /* Idle ships cannot be phasered */
    if (is_idle(j))
      continue;

    /*
     * (A, B) is the position of the possible target relative to me. */
    A = j->p_x - me->p_x;

    /*
     * Make sure (A,B) is close enough to me to probably be in
     * my phaser range.  (I am at (0,0).)                            */
    if ((A >= range) || (A <= -range))
      continue;
    B = j->p_y - me->p_y;
    if ((B >= range) || (B <= -range))
      continue;

    /* 
     * Possibly close enough for a hit.  
     * Check to make sure this target is closer to me than the current 
     * target.  This is also the exact check for phaser range, because
     * we initialized range_sq to 1+myphrange^2                      */
    this_range_sq = A*A + B*B;
    if (range_sq <= this_range_sq)
      continue;

    /*
     * The criterion for netrek phaser hits is that the phaser line
     * is within an arc such that it passes through the circle with
     * radius ZAPPLAYERDIST centered on the target.  This is equivalanet 
     * to determining if (E,F), the point on the phaser line nearest
     * the target, is within ZAPPLAYERDIST of the target.            */

    /*
     * Parametrically determine (E,F)
     * Here is a sketch of how s is derived:
     * We have two unknowns: E and F.  But we have two equations that
     * relate them:
     * (1) the slope of vector [C,D] has to be equal to the negative
     *     reciprical of the slope of [A-E,B-F]
     * (2) [E,F] = s * [C,D]
     * From (2) we derive the expression E=(F*C)/D.  We then plug that
     * into the equation derived from one.  Then, solve for F.       
     * The resulting equation is
     *   F = D * (A*C + B*D) / (C^2 * D^2)
     * One last step is to recall [C,D] has a constant length.       */

    s = (A*C + B*D) / (10.0*PHASEDIST * 10.0*PHASEDIST);

    if (s < 0)            /* KOC 12/95 - hack to stop 180 phaser bug */
      s = 0;              /* If enemy ship overlaps ship - autotarget it */

    E = (LONG) (C * s);
    /*
     * Check if (E,F) is probably close enough to (A,B) for a hit:   */
    dx = E - A;
    if ((dx > ZAPPLAYERDIST) || (dx < -ZAPPLAYERDIST))
      continue;
    F = (LONG) (D * s);
    dy = F - B;
    if ((dy > ZAPPLAYERDIST) || (dy < -ZAPPLAYERDIST))
      continue;
    
    /*
     * Probably close enough.
     * Now the circular range check:                                 */
    if (dx*dx + dy*dy <= ZAPPLAYERDIST*ZAPPLAYERDIST) {
      /* A hit! */
      pstatus = PHHIT;
      target_player = j;
      /*
       * Adjust range and range_sq in order to narrow the search area
       * for later hit checks.                                       */
      range_sq = this_range_sq;
      range = rint(sqrt((double)range_sq));
    }
  }
      

  /* See if the phaser hit a plasma torpedo: */
  for (t = firstPlasma; t <= lastPlasma; t++) {
    /* Don't check non-plasmas or my plasmas: */
    if ((t->t_status != TMOVE) || (t->t_owner == me->p_no))
      continue;
    if (!pingpong_plasma) {    /* Dont check if in pingpong plasma mode */
      if ((!(t->t_war & me->p_team)) &&
        (!(me->p_war & t->t_team)))
      continue;
    }
    /*
     * The code that checks for player hits (above) is practically
     * identical to this code.  So read the comments up there if you 
     * want to know what is going on down here. */
    A = t->t_x - me->p_x;
    if ((A >= range) || (A <= -range))
      continue;
    B = t->t_y - me->p_y;
    if ((B >= range) || (B <= -range))
      continue;
    this_range_sq = A*A + B*B;
    if (range_sq <= this_range_sq)
      continue;
    s = (A*C + B*D) / (10.0*PHASEDIST * 10.0*PHASEDIST);
    if (s < 0)            /* KOC 12/95 - hack to stop 180 phaser bug */
      continue;
    E = (LONG) (C * s);
    dx = E - A;
    if ((dx > ZAPPLASMADIST) || (dx < -ZAPPLASMADIST))
      continue;
    F = (LONG) (D * s);
    dy = F - B;
    if ((dy > ZAPPLASMADIST) || (dy < -ZAPPLASMADIST))
      continue;
    if (dx*dx + dy*dy <= ZAPPLASMADIST*ZAPPLASMADIST) {
      pstatus = PHHIT2;
      target_plasma = t;
      range_sq = this_range_sq;
      range = rint(sqrt((double)range_sq));
    }
  }

  /*
   * Adjust my phaser and me to reflect new conditions.  */
  mine->ph_status = pstatus;
  mine->ph_dir = course;
  mine->ph_fuse = me->p_ship.s_phaserfuse;

  me->p_fuel  -= myship->s_phasercost;
  me->p_wtemp += myship->s_phasercost / 10;


#ifdef LTD_STATS

  /* Phaser was fired at this point */
  ltd_update_phaser_fired(me);

#endif

  if (pstatus == PHMISS) {
#ifdef STURGEON
    /* s_phaserdamage varies, which varies end coordinate
    of a phaser miss, so we must calculate the coordinate */
    if (sturgeon) {
      mine->ph_x = me->p_x + (int) (PHASEDIST * me->p_ship.s_phaserdamage /
                        100 * Cos[course]);
      mine->ph_y = me->p_y + (int) (PHASEDIST * me->p_ship.s_phaserdamage /
                        100 * Sin[course]);
    }
#endif
    new_warning(37, "Phaser missed.");
    return;
  }

#ifdef LTD_STATS

  /* Phaser either hit a player or hit plasma, give credit for both cases */
  ltd_update_phaser_hit(me);

#endif


  if (pstatus == PHHIT) {
    mine->ph_target = target_player->p_no;
    mine->ph_damage = me->p_ship.s_phaserdamage *
      (1.0 - range / ((double) (myphrange)));

#ifdef LTD_STATS

    /* update phaser damage for shooter and victim */
    ltd_update_phaser_damage(me, target_player, mine->ph_damage);

#endif

    if (send_short && (target_player->p_no < 64) && (mine->ph_damage < 1024)) {
      swarning(PHASER_HIT_TEXT,
	       (target_player->p_no & 0x3f) | ((mine->ph_damage & 768) >> 2),
	       (unsigned char) mine->ph_damage & 0xff);
      return;
    }
    new_warning(UNDEF, "Phaser burst hit %s for %d points",
		target_player->p_name, mine->ph_damage);
    return;
  }

  /* pstatus == PHHIT2 */ {

#ifdef LTD_STATS

   /* phaser hit plasma */
   ltd_update_plasma_phasered(&(players[target_plasma->t_owner]));

#endif

   if (pingpong_plasma) {
	
	mine->ph_damage = me->p_ship.s_phaserdamage *
                          (1.0-((float) range/(float) myphrange));
     if ((target_plasma->t_war & me->p_team) 
			|| (me->p_hostile & target_plasma->t_team)) {
	int rand_plasmadir;
	int rand_plasmaturns;
	mine->ph_x = target_plasma->t_x;
	mine->ph_y = target_plasma->t_y;
	mine->ph_status = PHHIT2;
	target_plasma->t_damage+=mine->ph_damage/4;
	target_plasma->t_gspeed+=mine->ph_damage/100;
	srandom(getpid() * time((time_t *) 0));
	rand_plasmadir = random() % 9;
	rand_plasmaturns = random() % 9;
	if (rand_plasmadir < 8)
	    rand_plasmadir = 128;
	else rand_plasmadir = 80;
	if (rand_plasmaturns < 7) target_plasma->t_turns++;
	target_plasma->t_dir = (target_plasma->t_dir+rand_plasmadir)%256;
	players[target_plasma->t_owner].p_nplasmatorp--;
	target_plasma->t_owner=me->p_no;
	me->p_nplasmatorp++;
	target_plasma->t_team=me->p_team;
	target_plasma->t_war=me->p_hostile;
	target_plasma->t_fuse+=(mine->ph_damage * .25 + 5) * T_FUSE_SCALE;
	new_warning(UNDEF, "Ping-Pong!");
	}
     else {
	mine->ph_x = target_plasma->t_x;
	mine->ph_y = target_plasma->t_y;
	mine->ph_status = PHHIT2;
	target_plasma->t_damage+=mine->ph_damage;
	target_plasma->t_gspeed+=mine->ph_damage/100;
	target_plasma->t_turns++;
	target_plasma->t_dir = me->p_dir;
	players[target_plasma->t_owner].p_nplasmatorp--;
	target_plasma->t_owner=me->p_no;
	me->p_nplasmatorp++;
	target_plasma->t_team=me->p_team;
	target_plasma->t_war=me->p_hostile;
	target_plasma->t_fuse+=(mine->ph_damage * .25 + 5) * T_FUSE_SCALE;
	new_warning(UNDEF, "You deflected the plasma!"); }
   }

   if (!pingpong_plasma) {
     mine->ph_x = target_plasma->t_x;
     mine->ph_y = target_plasma->t_y;
     target_plasma->t_status = TDET;
     target_plasma->t_whodet = me->p_no;
     new_warning(38, "You destroyed the plasma torpedo!");
   }
  }
}

/* plasma.c -- Create a new plasma with a given direction and attributes
 */
#include "copyright.h"

#include <stdio.h>
#include <sys/types.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

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
  register int i;
  struct torp *j;
  struct   specialweapon *sw;
  struct   planet *l;
  struct   ship *myship;
  char     buf[80];
  int      destroyed, rnd;
  long     random();

  myship = &me->p_ship;
  if (sturgeon) {
    if (me->p_special < 0) {
      new_warning(UNDEF,"This ship is not equipped with any special weapons!");
      return;
    }
    sw = &me->p_weapons[me->p_special];

    if (sw->sw_number == 0) {
      sprintf(buf, "%s: none left", sw->sw_name);
      new_warning(UNDEF,buf);
      return;
    }
  }
  /* Normal plasmas and sturgeon SPECPLAS use similar checks */
  if(!sturgeon || (sturgeon && sw->sw_type == SPECPLAS))
#endif
  {
    if (weaponsallowed[WP_PLASMA]==0) {
      new_warning(17,"Plasmas haven't been invented yet.");
      return;
    }
#ifdef STURGEON
    if (!sturgeon) /* Plasma in sturgeon is independent of ship defaults, any ship can get it. */
#endif
    {
      if (me->p_ship.s_plasmacost < 0) {
        new_warning(18,"Weapon's Officer:  Captain, this ship is not armed with plasma torpedoes!");
        return;
      }
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
#ifdef STURGEON
    if (!sturgeon) /* Plasmas in sturgeon cost no fuel */
#endif
    {
      if (me->p_fuel < myship->s_plasmacost) {
        new_warning(22,"We don't have enough fuel to fire a plasma torpedo!");
        return;
      }
    }
    if (me->p_flags & PFREPAIR) {
      new_warning(23,"We cannot fire while our vessel is undergoing repairs.");
      return;
    }
    if ((me->p_cloakphase) && (me->p_ship.s_type != ATT)) {
#ifdef STURGEON
      if (sturgeon) {
        if (!me->p_upgradelist[UPG_FIRECLOAK]) {
          new_warning(UNDEF,"We don't have the proper upgrade to fire while cloaked, captain!");
          return;
        }
      }
      else
#endif
      {
        new_warning(24, "We are unable to fire while in cloak, captain!");
        return;
      }
    }
#ifdef STURGEON
    if (sturgeon) {
      for (i = me->p_no * MAXPLASMA, k = firstPlasmaOf(me);
           i < me->p_no * MAXPLASMA + MAXPLASMA; i++, k++) {
        if (k->t_status == TFREE)
          break;  /* found.  what if not found??? */
      }
    }
    else
#endif
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

#ifdef STURGEON
    if (sturgeon && sw->sw_number > 0)
      sw->sw_number--;
#endif

    me->p_nplasmatorp++;
#ifdef STURGEON
    if (!sturgeon)
#endif
      me->p_fuel -= myship->s_plasmacost;
#ifdef STURGEON
    if (sturgeon) {
      me->p_wtemp += (sw->sw_damage * 1.5 + 10); /* Heat weapons */
      if (sw->sw_number < 0)
        sprintf(buf, "Firing %s", sw->sw_name);
      else
        sprintf(buf, "Firing %s (%d left)", sw->sw_name, sw->sw_number);
      new_warning(UNDEF,buf);
    }
    else
#endif
      me->p_wtemp += (myship->s_plasmacost / 10) - 8;  /* Heat weapons */

#ifdef STURGEON
    if (sturgeon) {
      k->t_no = i;
      k->t_turns = sw->sw_turns;
      k->t_damage = sw->sw_damage;
      k->t_gspeed = (attributes & TVECTOR) ? torpGetVectorSpeed(me->p_dir, me->p_speed, course, sw->sw_speed) : sw->sw_speed  * WARP1;
      k->t_fuse = sw->sw_fuse;
      k->t_pldamage = 0;
    }
    else
#endif
    {
      k->t_turns = myship->s_plasmaturns;
      k->t_damage = myship->s_plasmadamage;
      k->t_gspeed = (attributes & TVECTOR) ? torpGetVectorSpeed(me->p_dir, me->p_speed, course, myship->s_plasmaspeed) : myship->s_plasmaspeed * WARP1;
      k->t_fuse = myship->s_plasmafuse;
    }
    k->t_status = TMOVE;
    k->t_type = TPLASMA;
    k->t_attribute = attributes;
    k->t_owner = me->p_no;
    k->t_x = me->p_x;
    k->t_y = me->p_y;
    k->t_dir    = ((myship->s_type == STARBASE) ||
                  (myship->s_type == ATT)) ? course : me->p_dir;
    k->t_war    = me->p_war;
    k->t_team   = me->p_team;
    k->t_whodet = NODET;
#if 0
    k->t_speed = k->t_gspeed / WARP1;
#endif
  } /* End normal plasmas/SPECPLAS */
#ifdef STURGEON
  if (sturgeon) {
    switch(sw->sw_type) {
      case SPECBOMB:
        if (me->p_nplasmatorp == MAXPLASMA) {
            new_warning(UNDEF,"We cannot drop a nuke yet!");
            break;
        }
        if (!(me->p_flags & PFORBIT)) {
            new_warning(UNDEF,"Must be orbiting to drop nuke!");
            break;
        }
        l = &planets[me->p_planet];
        if (me->p_team == l->pl_owner) {
            new_warning(UNDEF,"We can't nuke our own planet!");
            break;
        }
        if (!((me->p_swar | me->p_hostile) & l->pl_owner)) {
            new_warning(UNDEF,"We can't nuke a friendly planet!");
            break;
        }

        if (sw->sw_number > 0)
            sw->sw_number--;
        me->p_nplasmatorp++;
        me->p_flags &= ~(PFSHIELD | PFREPAIR | PFBEAMDOWN | PFBEAMUP | PFBOMB);
        me->p_swar |= l->pl_owner;
        me->p_ship.s_maxarmies += me->p_special - 5;

        for (i = me->p_no * MAXPLASMA, k = firstPlasmaOf(me);
            i < me->p_no * MAXPLASMA + MAXPLASMA; i++, k++) {
                if (k->t_status == TFREE)
                    break;  /* found.  what if not found??? */
        }
        k->t_no = i;
        k->t_status = TMOVE;
        k->t_type = TPLASMA;
        k->t_owner = me->p_no;
        k->t_team = me->p_team;
        k->t_x = l->pl_x;
        k->t_y = l->pl_y;
        k->t_dir = k->t_gspeed = k->t_war = 0;
        k->t_damage = 1;
        k->t_fuse = 100;
        k->t_turns = 0;
        k->t_plbombed = l->pl_no;
        k->t_pldamage = 0;

        destroyed = sw->sw_damage;
        rnd = random() % 100;
        if (rnd < 5) {                                  /* uh oh... */
            new_warning(UNDEF,"Nuke explodes in bomb bay!");
            destroyed = 0;
            k->t_x = me->p_x;
            k->t_y = me->p_y;
            k->t_damage = sw->sw_damage * 10;
        }
        else if (rnd < 10) {
            new_warning(UNDEF,"The nuclear warhead failed to detonate.");
            destroyed = 0;
            k->t_status = TFREE;
            me->p_nplasmatorp--;
        }
        else if (rnd < 23) {
            sprintf(buf, "Nuke landed in lightly populated area.");
            destroyed -= 2;
        }
        else if (rnd < 76) {
            new_warning(UNDEF,"Normal effects.");
        }
        else if (rnd < 80) {
            new_warning(UNDEF,"Nuclear fireball engulfs an additional army.");
            destroyed += 1;
        }
        else if (rnd < 84) {
            new_warning(UNDEF,"Radioactive fallout kills two additional armies.");
            destroyed += 2;
        }
        else if (rnd < 88) {
            new_warning(UNDEF,"Beta rays kill an additional five armies!");
            destroyed += 5;
        }
        else if (rnd < 92) {
            new_warning(UNDEF,"Deadly gamma rays kill ten more armies!");
            destroyed += 10;
        }
        else if (rnd < 96) {
            new_warning(UNDEF,"Dirty bomb.  Double yield.");
            destroyed *= 2;
        }
        else {
            new_warning(UNDEF,"You hit a nuclear stockpile!  Triple yield!");
            destroyed *= 3;
            k->t_damage = destroyed;
        }
        if (destroyed > l->pl_armies)
            destroyed = l->pl_armies;

        k->t_pldamage = destroyed;
        k->t_whodet = me->p_no;
        k->t_status = TDET;             /* moved down here for safety...  */
                                        /* daemon will ignore until ready */
        apply_upgrade(100, me, 0); /* Dummy upgrade just to send ship cap */

        break;

      case SPECTORP:
      case SPECMINE:
        if (me->p_ntorp == MAXTORP) {
            new_warning(UNDEF,"Our computers limit us to having 8 live torpedos at a time captain!");
            return;
        }
        if (me->p_fuel < sw->sw_fuelcost) {
            new_warning(UNDEF,"We don't have enough fuel to launch that!");
            return;
        }
        if (me->p_flags & PFREPAIR) {
            new_warning(UNDEF,"We cannot fire while our vessel is in repair mode.");
            return;
        }
        if ((me->p_cloakphase) && (me->p_ship.s_type != ATT)) {
            new_warning(UNDEF,"We cannot launch while cloaked, captain!");
            return;
        }

        if (sw->sw_number > 0)
        {
            sw->sw_number--;
            sprintf(buf, "Launching %s (%d left)", sw->sw_name, sw->sw_number);
            new_warning(UNDEF,buf);
        }

        me->p_ntorp++;
        me->p_fuel -= sw->sw_fuelcost;
        me->p_wtemp += (sw->sw_damage/5);  /* Heat weapons */

        for (i = me->p_no * MAXTORP, j = firstTorpOf(me);       /* Find a free torp */
            i < me->p_no * MAXTORP + MAXTORP; i++, j++) {
                if (j->t_status == TFREE)
                    break;
        }

        /* Setup data in new torp (fighter) */
        j->t_no = i;
        j->t_status = TMOVE;
        j->t_type = TPHOTON;
        /* Plasmas default to 0 attributes, need to set them up for mines
           and drones.  Treat them like a regular torp. */
        j->t_attribute |= (TWOBBLE | TOWNERSAFE | TDETTEAMSAFE);
        if (sw->sw_type == SPECMINE)
            j->t_spinspeed = 16;
        else
            j->t_spinspeed = 0;
        j->t_owner = me->p_no;
        j->t_team = me->p_team;
        j->t_x = me->p_x;
        j->t_y = me->p_y;
        j->t_dir = course;
        j->t_damage = sw->sw_damage;
        j->t_gspeed = sw->sw_speed * WARP1;
        j->t_war = me->p_war;
        j->t_fuse = sw->sw_fuse + (random() % 20);
        j->t_turns = sw->sw_turns;
        break;
      default:
        break;
    }
  }
#endif
#ifdef LTD_STATS

  /* At this point, a plasma was fired */
  if (status->tourn) {
    ltd_update_plasma_fired(me);
  }

#endif

}

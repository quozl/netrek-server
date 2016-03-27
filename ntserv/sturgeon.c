#include <stdio.h>
#include <math.h>
#include "copyright.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "sturgeon.h"
#include "util.h"

#ifdef STURGEON

static void sturgeon_special_begin(struct player *me)
{
  int i;

  for (i = 0; i < NUMSPECIAL; i++)
    me->p_weapons[i].sw_number = 0;

  strcpy (me->p_weapons[0].sw_name, "Pseudoplasma");
  me->p_weapons[0].sw_type = SPECPLAS;
  me->p_weapons[0].sw_fuelcost = 0;
  me->p_weapons[0].sw_damage = 0;
  me->p_weapons[0].sw_fuse = 30;
  me->p_weapons[0].sw_speed = 15;
  me->p_weapons[0].sw_turns = 1;

  for (i = 1; i <= 5; i++) {
    sprintf(me->p_weapons[i].sw_name, "Type %d Plasma", i);
    me->p_weapons[i].sw_type = SPECPLAS;
    me->p_weapons[i].sw_fuelcost = 0;
    me->p_weapons[i].sw_damage = 25 * (i + 1);
    me->p_weapons[i].sw_fuse = 30;
    me->p_weapons[i].sw_speed = 15;
    me->p_weapons[i].sw_turns = 1;
  }

  strcpy(me->p_weapons[6].sw_name, "10 megaton warhead");
  me->p_weapons[6].sw_damage = 2;

  strcpy(me->p_weapons[7].sw_name, "20 megaton warhead");
  me->p_weapons[7].sw_damage = 5;

  strcpy(me->p_weapons[8].sw_name, "50 megaton warhead");
  me->p_weapons[8].sw_damage = 10;

  strcpy(me->p_weapons[9].sw_name, "100 megaton warhead");
  me->p_weapons[9].sw_damage = 25;

  for (i = 6; i <= 9; i++) {
    me->p_weapons[i].sw_type = SPECBOMB;
    me->p_weapons[i].sw_fuelcost = 0;
    me->p_weapons[i].sw_fuse = 1;
    me->p_weapons[i].sw_speed = 0;
    me->p_weapons[i].sw_turns = 0;
  }

  strcpy(me->p_weapons[10].sw_name, "Suicide Drones");
  me->p_weapons[10].sw_type = SPECTORP;
  me->p_weapons[10].sw_fuelcost = 500;
  me->p_weapons[10].sw_damage = 60;
  me->p_weapons[10].sw_speed = 5;
  me->p_weapons[10].sw_fuse = 150;
  me->p_weapons[10].sw_turns = 2;

  strcpy(me->p_weapons[11].sw_name, "Mines");
  me->p_weapons[11].sw_type = SPECMINE;
  me->p_weapons[11].sw_fuelcost = 250;
  me->p_weapons[11].sw_damage = 200;
  me->p_weapons[11].sw_speed = 2;
  me->p_weapons[11].sw_fuse = 600;
  me->p_weapons[11].sw_turns = 0;
}

void sturgeon_hook_enter(struct player *me, int s_type, int tno)
{
  char addrbuf[10], buf[80];
  int i, team;

  /* Remove upgrades for bases */
  if (s_type == STARBASE) {
    me->p_upgrades = 0.0;
    for (i = 0; i < NUMUPGRADES; i++)
      me->p_upgradelist[i] = 0;
  }

  /* If non-base ship hasn't spent it's base rank credit upgrades,
     leave them on.  And in the case of genocide/conquer, leave them
     on too */
  else if (me->p_upgrades > (float) me->p_stats.st_rank
           && me->p_whydead != KGENOCIDE
           && me->p_whydead != KWINNER) {
    if (sturgeon_lite) {
      /* Lose most costly upgrade */
      double highestcost = 0.0, tempcost = 0.0;
      int highestupgrade = 0;
      for (i = 1; i < NUMUPGRADES; i++) {
        if (me->p_upgradelist[i] > 0) {
          tempcost = baseupgradecost[i] + (me->p_upgradelist[i] - 1.0) * adderupgradecost[i];
          if (tempcost > highestcost) {
            highestupgrade = i;
            highestcost = tempcost;
          }
        }
      }
      /* Only remove it if rank credit insufficient to cover all
         upgrades */
      if (me->p_upgrades > (float) me->p_stats.st_rank) {
        me->p_upgradelist[highestupgrade]--;
        me->p_upgrades -= highestcost;
      }
    } else {
      /* Strip off upgrades until total upgrade amount is less than or
         equal to rank credit.  Run through normal upgrades first,
         then strip off the one time upgrades last, only if
         necessary. */
      for (i = 1; i < NUMUPGRADES; i++) {
        if (me->p_upgradelist[i] > 0) {
          while (me->p_upgradelist[i] != 0) {
            me->p_upgradelist[i]--;
            me->p_upgrades -= baseupgradecost[i] + me->p_upgradelist[i] * adderupgradecost[i];
            if (me->p_upgrades <= (float) me->p_stats.st_rank)
              break;
          }
        }
        if (me->p_upgrades <= (float) me->p_stats.st_rank)
          break;
      }
      /* If for some reason we still are over, just set upgrades to
         zero.  Should never happen, but here to be safe */
      if (me->p_upgrades > (float) me->p_stats.st_rank)
        me->p_upgrades = 0;
    }
  }
  /* Calculate new rank credit */
  me->p_rankcredit = (float) me->p_stats.st_rank - me->p_upgrades;
  if (me->p_rankcredit < 0.0)
    me->p_rankcredit = 0.0;
  /* Now we need to go through the upgrade list and reapply all
     upgrades that are left, as default ship settings have been reset
     */
  for (i = 1; i < NUMUPGRADES; i++) {
    if (me->p_upgradelist[i] > 0)
      sturgeon_apply_upgrade(i, me, me->p_upgradelist[i]);
  }
  me->p_special = -1;
  sturgeon_special_begin(me);
  /* AS get unlimited mines */
  if (s_type == ASSAULT) {
    me->p_weapons[11].sw_number = -1;
    me->p_special = 11;
  }
  /* Get team (needed for starbase enter messages) */
  if ((tno == 4) || (tno == 5) || (me->w_queue == QU_GOD_OBS))
    team = 0;
  else
    team = (1 << tno);
  /* SB gets unlimited pseudoplasma, type 5 plasma, and suicide drones */
  if (s_type == STARBASE) {
    me->p_weapons[0].sw_number = -1;
    me->p_weapons[5].sw_number = -1;
    me->p_weapons[10].sw_number = -1;
    me->p_special = 10;
    sprintf(buf, "%s (%c%c) is now a Starbase",
            me->p_name, teamlet[team], shipnos[me->p_no]);
    strcpy(addrbuf, "GOD->ALL");
    pmessage(0, MALL, addrbuf, buf);
  }
  /* Galaxy class gets unlimited pseudoplasma */
  if (s_type == SGALAXY) {
    me->p_weapons[0].sw_number = -1;
    me->p_special = 0;
  }
  /* ATT gets unlimited everything */
  if (s_type == ATT) {
    for (i = 0; i <= 10; i++)
      me->p_weapons[i].sw_number = -1;
    me->p_special = 10;
  }
}

int sturgeon_hook_refit_0(struct player *me, int type)
{
  char addrbuf[80];

  if (!upgradeable) return 0;
  if (type != me->p_ship.s_type) return 0;
  /* No upgrades for starbases */
  if (me->p_ship.s_type == STARBASE) return 0;

  sprintf(addrbuf, "UPG->%c%c ", teamlet[me->p_team], shipnos[me->p_no]);
  pmessage(me->p_no, MINDIV, addrbuf,
           "Upgrade: 0=abort, 1=normal refit, 2=shields/hull, 3=engines");
  pmessage(me->p_no, MINDIV, addrbuf,
           "         4=weapons, 5=special weapons, 6=one time, 7=misc");
  new_warning(UNDEF, "Please make an upgrade selection");
  me->p_upgrading = 1;
  return 1;
}

void sturgeon_hook_refit_1(struct player *me, int type)
{
  int i;
  char buf[80];
  char addrbuf[80];

        /* Convert upgrades back to kills, but dont give credit for upgrades bought with rank */
/*
        if (me->p_upgrades > 0.0) {
            float rankcredit;
            rankcredit = (float)me->p_stats.st_rank - me->p_rankcredit;
            if (rankcredit < 0)
                rankcredit = 0;
            me->p_kills += me->p_upgrades - rankcredit;
            me->p_upgrades = 0.0;
        }
        me->p_rankcredit = (float) me->p_stats.st_rank;
        for (i = 0; i < NUMUPGRADES; i++)
            me->p_upgradelist[i] = 0;
*/

  for (i = 0; i < NUMSPECIAL; i++)
    me->p_weapons[i].sw_number = 0;
  if (type == STARBASE) {
    /* Remove upgrades for bases */
    me->p_upgrades = 0.0;
    for (i = 0; i < NUMUPGRADES; i++)
      me->p_upgradelist[i] = 0;
  } else {
    /* Now we need to go through the upgrade list and reapply all upgrades
       that are left, as default ship settings have been reset */
    for (i = 1; i < NUMUPGRADES; i++) {
      if (me->p_upgradelist[i] > 0)
	sturgeon_apply_upgrade(i, me, me->p_upgradelist[i]);
    }
  }
  switch (type) {
    /* AS get unlimited mines */
  case ASSAULT:
    me->p_weapons[11].sw_number = -1;
    me->p_special = 11;
    break;
    /* SB gets unlimited pseudoplasma, type 5 plasma, and suicide drones */
  case STARBASE:
    me->p_weapons[0].sw_number = -1;
    me->p_weapons[5].sw_number = -1;
    me->p_weapons[10].sw_number = -1;
    me->p_special = 10;
    sprintf(buf, "%s (%c%c) is now a Starbase",
	    me->p_name, teamlet[me->p_team], shipnos[me->p_no]);
    strcpy(addrbuf, "GOD->ALL");
    pmessage(0, MALL, addrbuf, buf);
    break;
    /* Galaxy class gets unlimited pseudoplasma */
  case SGALAXY:
    me->p_weapons[0].sw_number = -1;
    me->p_special = 0;
    break;
    /* ATT gets unlimited everything */
  case ATT:
    for (i = 0; i <= 10; i++)
      me->p_weapons[i].sw_number = -1;
    me->p_special = 10;
    break;
  default:
    me->p_special = -1;
    break;
  }
}

static void sturgeon_weapon_switch()
{
  char buf[80];
  int t = me->p_special;

  if (!sturgeon_special_weapons) {
    new_warning(UNDEF, "Special weapons have been disabled.");
    return;
  }

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

int sturgeon_hook_coup()
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

static int upgrade(int type, struct player *me, char *item, double base, double adder)
{
    double  kills_req, kills_gained;
    char    buf[80];

    if (me->p_undo_upgrade) {
        if (me->p_upgradelist[type]) {
            me->p_upgradelist[type]--;
            kills_gained = baseupgradecost[type] + me->p_upgradelist[type] * adderupgradecost[type];
            /* Try to increment rank credit first */
            if (((float) me->p_stats.st_rank - me->p_rankcredit) >= kills_gained
              && me->p_upgrades <= (float) me->p_stats.st_rank)
              me->p_rankcredit += kills_gained;
            else
              me->p_kills += kills_gained;
            if (type < UPG_OFFSET)
              sprintf(buf, "Ship's %s decreased.", item);
            else /* 1 time upgrades */
              sprintf(buf, "You have lost the %s upgrade!", item);
            new_warning(UNDEF,buf);
            sturgeon_unapply_upgrade(type, me, 1);
            me->p_upgrades -= kills_gained;
            me->p_undo_upgrade = 0;
            return 0;
        } else {
            new_warning(UNDEF,"Undo upgrade failed - you don't have that upgrade.");
            return 0;
        }
    }
    if (type > 0)
        kills_req = baseupgradecost[type] +
            me->p_upgradelist[type] * adderupgradecost[type];
    else
        kills_req = base + me->p_upgradelist[type] * adder;

    me->p_upgrading = 0;
    me->p_flags &= ~(PFREFIT);
    /* Either kills or rank credits can be spent on upgrades */
    if (type > 0) {
        if (me->p_kills + me->p_rankcredit < kills_req) {
            if (type < UPG_OFFSET)
                sprintf(buf, "You need %0.2f kills/rank credit to increase your ship's %s.",
                        kills_req, item);
            else /* 1 time upgrades */
                sprintf(buf, "You need %0.2f kills/rank credit to gain the %s upgrade.",
                    kills_req, item);
            new_warning(UNDEF,buf);
            return(0);
        }
        if (sturgeon_maxupgrades) {
            if (me->p_upgrades >= sturgeon_maxupgrades) {
               sprintf(buf, "You are at the upgrade limit and cannot buy any more upgrades.");
                new_warning(UNDEF,buf);
                return 0;
            } else if ((me->p_upgrades + kills_req) > sturgeon_maxupgrades) {
                sprintf(buf, "That would put you over the upgrade limit, try buying something less expensive.");
                new_warning(UNDEF,buf);
                return 0;
            }
        }
    } else {
        /* Only actual kills can buy temporary items */
        if (me->p_kills < kills_req) {
            sprintf(buf, "You need %0.2f kills to increase your ship's %s.",
                    kills_req, item);
            new_warning(UNDEF,buf);
            return 0;
        }
        /* Limit on shield overcharge */
        if (strstr(item, "shields")) {
            long shieldboost;

            shieldboost = (100 * me->p_shield)/(me->p_ship.s_maxshield);
            if (shieldboost >= 200) {
                sprintf(buf, "Your shields (%ld percent normal) cannot be overcharged any more.", shieldboost);
                new_warning(UNDEF,buf);
                return 0;
            }
        }
    }
    me->p_flags |= PFREFITTING;
    if (type) {
        me->p_upgrades += kills_req;
        me->p_upgradelist[type]++;
    }
    /* Decrement any rank credit first, for actual upgrades */
    if (type > 0 && me->p_rankcredit >= kills_req) {
        me->p_rankcredit -= kills_req;
    } else if (type > 0 && me->p_rankcredit > 0.0) {
        me->p_kills -= (kills_req - me->p_rankcredit);
        me->p_rankcredit = 0.0;
    } else
        me->p_kills -= kills_req;

    rdelay = me->p_updates + (kills_req * 10);
    if (type < UPG_OFFSET)
        sprintf(buf, "Ship's %s increased.", item);
    else /* 1 time upgrades */
        sprintf(buf, "You have gained the %s upgrade!", item);
    new_warning(UNDEF,buf);
    return 1;
}

/* Function to actually apply all upgrades */
void sturgeon_apply_upgrade(int type, struct player *j, int multiplier)
{
    int i = multiplier;

    switch (type) {
    case UPG_TEMPSHIELD: /* Temporary shield boost */
        j->p_shield += 50 * i;
        break;
    case UPG_PERMSHIELD: /* Permanent shield boost */
        j->p_ship.s_maxshield += 10 * i;
        break;
    case UPG_HULL: /* Permanent hull boost */
        j->p_ship.s_maxdamage += 10 * i;
        break;
    case UPG_FUEL: /* Fuel capacity */
        j->p_ship.s_maxfuel += 250 * i;
        break;
    case UPG_RECHARGE: /* Fuel recharge rate */
        j->p_ship.s_recharge += 1 * i;
        break;
    case UPG_MAXWARP: /* Maximum warp speed */
        j->p_ship.s_maxspeed += 1 * i;
        break;
    case UPG_ACCEL: /* Acceleration rate */
        j->p_ship.s_accint += 10 * i;
        break;
    case UPG_DECEL: /* Deceleration rate */
        j->p_ship.s_decint += 10 * i;
        break;
    case UPG_ENGCOOL: /* Engine cool rate */
        j->p_ship.s_egncoolrate += 1 * i;
        break;
    case UPG_PHASER: /* Phaser efficiency */
        j->p_ship.s_phaserdamage += 2 * i;
        break;
    case UPG_TORPSPEED: /* Photon speed */
        j->p_ship.s_torpspeed += 1 * i;
        break;
    case UPG_TORPFUSE: /* Photon fuse */
        j->p_ship.s_torpfuse += 1 * i;
        break;
    case UPG_WPNCOOL: /* Weapon cooling rate */
        j->p_ship.s_wpncoolrate += 1 * i;
        break;
    case UPG_CLOAK: /* Cloak device efficiency - unused */
        break;
    case UPG_TPSTR: /* Tractor/pressor strength */
        j->p_ship.s_tractstr += 100 * i;
        break;
    case UPG_TPRANGE: /* Tractor/pressor range */
        j->p_ship.s_tractrng += .05 * i;
        break;
    case UPG_REPAIR: /* Damage control efficiency */
        j->p_ship.s_repair += 10 * i;
        break;
    case UPG_FIRECLOAK: /* Fire while cloaked */
    case UPG_DETDMG: /* Det own torps for damage */
    default:
        break;
    }

    /* Notify client of new ship stats */
    sndShipCap();
}

/* Function to unapply upgrades */
void sturgeon_unapply_upgrade(int type, struct player *j, int multiplier)
{
    int i = multiplier;

    switch (type) {
    case UPG_PERMSHIELD: /* Permanent shield boost */
        j->p_ship.s_maxshield -= 10 * i;
        j->p_shield -= 10 * i;
        break;
    case UPG_HULL: /* Permanent hull boost */
        j->p_ship.s_maxdamage -= 10 * i;
        break;
    case UPG_FUEL: /* Fuel capacity */
        j->p_ship.s_maxfuel -= 250 * i;
        break;
    case UPG_RECHARGE: /* Fuel recharge rate */
        j->p_ship.s_recharge -= 1 * i;
        break;
    case UPG_MAXWARP: /* Maximum warp speed */
        j->p_ship.s_maxspeed -= 1 * i;
        break;
    case UPG_ACCEL: /* Acceleration rate */
        j->p_ship.s_accint -= 10 * i;
        break;
    case UPG_DECEL: /* Deceleration rate */
        j->p_ship.s_decint -= 10 * i;
        break;
    case UPG_ENGCOOL: /* Engine cool rate */
        j->p_ship.s_egncoolrate -= 1 * i;
        break;
    case UPG_PHASER: /* Phaser efficiency */
        j->p_ship.s_phaserdamage -= 2 * i;
        break;
    case UPG_TORPSPEED: /* Photon speed */
        j->p_ship.s_torpspeed -= 1 * i;
        break;
    case UPG_TORPFUSE: /* Photon fuse */
        j->p_ship.s_torpfuse -= 1 * i;
        break;
    case UPG_WPNCOOL: /* Weapon cooling rate */
        j->p_ship.s_wpncoolrate -= 1 * i;
        break;
    case UPG_CLOAK: /* Cloak device efficiency - unused */
        break;
    case UPG_TPSTR: /* Tractor/pressor strength */
        j->p_ship.s_tractstr -= 100 * i;
        break;
    case UPG_TPRANGE: /* Tractor/pressor range */
        j->p_ship.s_tractrng -= .05 * i;
        break;
    case UPG_REPAIR: /* Damage control efficiency */
        j->p_ship.s_repair -= 10 * i;
        break;
    case UPG_TEMPSHIELD: /* Temp shield boost */
    case UPG_FIRECLOAK: /* Fire while cloaked */
    case UPG_DETDMG: /* Det own torps for damage */
    default:
        break;
    }

    /* Notify client of new ship stats */
    sndShipCap();
}

int sturgeon_hook_set_speed(int speed)
{
    struct ship tmpship;
    char buf[100];
    char addrbuf[10];
    int i;

    if (sturgeon && me->p_upgrading) {
        if (speed == 0) {
            new_warning(UNDEF,"Upgrade aborted");
            me->p_flags &= ~(PFREFIT);
            me->p_upgrading = 0;
            return 0;
        }
        sprintf(addrbuf,"UPG->%c%c ", teamlet[me->p_team], shipnos[me->p_no]);
        getship(&tmpship, me->p_ship.s_type);
        switch (me->p_upgrading) {
        case 1:                 /* first level menu */
            switch(speed) {
            case 1: me->p_shield = me->p_ship.s_maxshield;
                    me->p_fuel = me->p_ship.s_maxfuel;
                    me->p_damage = me->p_wtemp = me->p_etemp = 0;
                    me->p_flags &= ~(PFREFIT);
                    me->p_flags |= PFREFITTING;
                    rdelay = me->p_updates + 30;
                    new_warning(UNDEF,"Ship being overhauled...");
                    me->p_upgrading = 0;
                    break;
            case 2: pmessage(me->p_no, MINDIV, addrbuf,"Shields/Hull: 0=abort, 1=perm shield, 2=temp shield, 3=perm hull");
                    me->p_upgrading = 2;
                    break;
            case 3: pmessage(me->p_no, MINDIV, addrbuf,"Engines: 0=abort, 1=fuel, 2=rechg, 3=maxwarp, 4=acc, 5=dec, 6=cool");
                    me->p_upgrading = 3;
                    break;
            case 4: pmessage(me->p_no, MINDIV, addrbuf,"Weapons: 0=abort, 1=phaser dmg, 2=torp speed, 3=torp fuse, 4=cooling");
                    me->p_upgrading = 4;
                    break;
            case 5: if (sturgeon_special_weapons) {
                        pmessage(me->p_no, MINDIV, addrbuf,"Special: 0=abort, 1=plasmas, 2=nukes, 3=drones, 4=mines, 5=inventory");
                        me->p_upgrading = 5;
                        break;
                    }
                    else {
                        pmessage(me->p_no, MINDIV, addrbuf,"Special weapons are disabled.");
                        me->p_flags &= ~(PFREFIT);
                        me->p_upgrading = 0;
                        break;
                    }
            case 6: pmessage(me->p_no, MINDIV, addrbuf,"One time: 0=abort, 1=fire while cloaked, 2=det own for damage");
                    me->p_upgrading = 6;
                    break;
            case 7: pmessage(me->p_no, MINDIV, addrbuf,"Misc: 0=abort, 1=cloak, 2=tractor strength, 3=tractor range");
                    pmessage(me->p_no, MINDIV, addrbuf,"      4=repair rate, 5=current status 6=undo next upgrade");
                    me->p_upgrading = 7;
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
	    }
	    break;
        case 2:                 /* shield/hull upgrade menu */
            switch (speed) {
            case 1: if (upgrade(UPG_PERMSHIELD, me, "shield max", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_PERMSHIELD, me, 1);
                    break;
            case 2: if (upgrade(UPG_TEMPSHIELD, me, "shields (temporarily)", 1.0, 0.0))
                        sturgeon_apply_upgrade(UPG_TEMPSHIELD, me, 1);
                    break;
            case 3: if (upgrade(UPG_HULL, me, "hull max", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_HULL, me, 1);
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;
        case 3:                 /* engine upgrade menu */
            switch (speed) {
            case 1: if (upgrade(UPG_FUEL, me, "fuel capacity", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_FUEL, me, 1);
                    break;
            case 2: if (upgrade(UPG_RECHARGE, me, "fuel collector efficiency", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_RECHARGE, me, 1);
                    break;
            case 3: if (upgrade(UPG_MAXWARP, me, "maximum warp speed", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_MAXWARP, me, 1);
                    break;
            case 4: if (upgrade(UPG_ACCEL, me, "acceleration rate", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_ACCEL, me, 1);
                    break;
            case 5: if (upgrade(UPG_DECEL, me, "deceleration rate", 0.0, 0.0))
                         sturgeon_apply_upgrade(UPG_DECEL, me, 1);
                    break;
            case 6: if (upgrade(UPG_ENGCOOL, me, "engine cooling rate", 0.0, 0.0))
                         sturgeon_apply_upgrade(UPG_ENGCOOL, me, 1);
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;
        case 4:                 /* weapon upgrade menu */
            switch (speed) {
            case 1: if (upgrade(UPG_PHASER, me, "phaser damage", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_PHASER, me, 1);
                    break;
            case 2: if (upgrade(UPG_TORPSPEED, me, "photon speed", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_TORPSPEED, me, 1);
                    break;
            case 3: if (upgrade(UPG_TORPFUSE, me, "photon fuse", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_TORPFUSE, me, 1);
                    break;
            case 4: if (upgrade(UPG_WPNCOOL, me, "weapon cooling rate", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_WPNCOOL, me, 1);
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;
        case 5:                 /* special weapons menu */
            switch (speed) {
            case 1: pmessage(me->p_no, MINDIV, addrbuf,"Plasmas: 0=abort, 1-5 = type 1-5, 9=pseudoplasma");
                    me->p_upgrading = 8;
                    break;
            case 2: pmessage(me->p_no, MINDIV, addrbuf,"Nuclear: 0=abort, 1=small, 2=med, 3=big, 4=huge");
                    me->p_upgrading = 9;
                    break;
            case 3:
                    if (me->p_weapons[10].sw_number < 0) {
                        pmessage(me->p_no, MINDIV, addrbuf,"You don't need to buy drones.");
                        break;
                    }
                    if (upgrade(0, me, "drone inventory", 1.0, 0.0)) {
                        me->p_weapons[10].sw_number += 3;
                        sprintf(buf, "%s (%d)", me->p_weapons[10].sw_name,
                            me->p_weapons[10].sw_number);
                        pmessage(me->p_no, MINDIV, addrbuf,buf);
                        me->p_special = 10;
                    }
                    break;
            case 4:
                    if (me->p_weapons[11].sw_number < 0) {
                        pmessage(me->p_no, MINDIV, addrbuf,"You don't need to buy mines.");
                        break;
                    }
                    if (upgrade(0, me, "mine inventory", 1.0, 0.0)) {
                        me->p_weapons[11].sw_number += 2;
                        sprintf(buf, "%s (%d)", me->p_weapons[11].sw_name,
                            me->p_weapons[11].sw_number);
                        pmessage(me->p_no, MINDIV, addrbuf,buf);
                        me->p_special = 11;
                    }
                    break;
            case 5:
                    if (me->p_special < 0) {
                        new_warning(UNDEF,"This ship has no special weapons");
                        break;
                    }
                    pmessage(me->p_no, MINDIV, addrbuf,"Inventory:");
                    for (i=0; i < NUMSPECIAL; i++)
                        if (me->p_weapons[i].sw_number != 0) {
                            if (me->p_weapons[i].sw_number < 0)
                                sprintf(buf, " inf %s", me->p_weapons[i].sw_name);
                            else
                                sprintf(buf, "%4d %s", me->p_weapons[i].sw_number,
                                    me->p_weapons[i].sw_name);
                            pmessage(me->p_no, MINDIV, addrbuf,buf);
                        }
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;
        case 6:                 /* One time upgrade menu */
            switch (speed) {
            case 1:
                    if (me->p_upgradelist[UPG_FIRECLOAK] && !me->p_undo_upgrade) {
                        pmessage(me->p_no, MINDIV, addrbuf,"You can already fire while cloaked.");
                        break;
                    }
                    if (upgrade(UPG_FIRECLOAK, me, "fire while cloak", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_FIRECLOAK, me, 1);
                    break;
            case 2:
                    if (me->p_upgradelist[UPG_DETDMG] && !me->p_undo_upgrade) {
                        pmessage(me->p_no, MINDIV, addrbuf,"You can already det own torps for damage.");
                        break;
                    }
                    if (upgrade(UPG_DETDMG, me, "det own torps for damage", 0.0, 0.0))
                        sturgeon_apply_upgrade(UPG_DETDMG, me, 1);
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;

        case 7:                 /* miscellaneous systems menu */
            switch (speed) {
            case 1:
                new_warning(UNDEF,"Sorry, cloak upgrades have been disabled.");
                me->p_flags &= ~(PFREFIT);
                me->p_upgrading = 0;
                break;

                if (me->p_ship.s_cloakcost) {
                    if (upgrade(UPG_CLOAK, me, "cloaking device efficiency", 0.0, 0.0)) {
                        if (me->p_ship.s_cloakcost == 1)
                            me->p_ship.s_cloakcost = 0;
                        else
                            me->p_ship.s_cloakcost =
                                ceil(me->p_ship.s_cloakcost / 2.0);
                    }
                }
                else
                    new_warning(UNDEF,"Cloaking device efficiency already at maximum.");
                break;
            case 2:
                if (upgrade(UPG_TPSTR, me, "tractor/pressor strength", 0.0, 0.0))
                    sturgeon_apply_upgrade(UPG_TPSTR, me, 1);
                break;
            case 3:
                if (upgrade(UPG_TPRANGE, me, "tractor/pressor range", 0.0, 0.0))
                    sturgeon_apply_upgrade(UPG_TPRANGE, me, 1);
                break;
            case 4:
                if (upgrade(UPG_REPAIR, me, "damage control efficiency", 0.0, 0.0))
                    sturgeon_apply_upgrade(UPG_REPAIR, me, 1);
                break;
            case 5:
                if (me->p_upgrades == 0.0) {
                    new_warning(UNDEF,"This ship has no upgrades");
                    break;
                }
                pmessage(me->p_no, MINDIV, addrbuf,"Current upgrades (cost for next):");
                for (i = 1; i < NUMUPGRADES; i++) {
                    if (me->p_upgradelist[i] > 0) {
                        if (i < UPG_OFFSET)
                            sprintf(buf, "%4d (%6.2f) %s", me->p_upgradelist[i],
                                baseupgradecost[i] + me->p_upgradelist[i] *
                                adderupgradecost[i], upgradename[i]);
                        else /* 1 time upgrades */
                            sprintf(buf, "%s upgrade", upgradename[i]);
                        pmessage(me->p_no, MINDIV, addrbuf,buf);
                    }
                }
                break;
            case 6:
                if (me->p_undo_upgrade) {
                    me->p_undo_upgrade = 0;
                    new_warning(UNDEF,"Undo upgrade flag removed.");
                }
                else {
                    me->p_undo_upgrade = 1;
                    new_warning(UNDEF,"Undo upgrade flag set - next upgrade purchased will be removed instead.");
                }
                break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;
        case 8:                 /* Plasma torp menu */
            switch (speed) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
                    if (upgrade(0, me, "plasma torp inventory", 2.0, 0.0)) {
                        me->p_weapons[speed].sw_number += 12/speed;
                        sprintf(buf, "%s (%d)", me->p_weapons[speed].sw_name,
                            me->p_weapons[speed].sw_number);
                        pmessage(me->p_no, MINDIV, addrbuf,buf);
                        me->p_special = speed;
                    }
                    break;
            case 9:
                    if (upgrade(0, me, "pseudoplasma inventory", 1.0, 0.0)) {
                        me->p_weapons[0].sw_number += 12;
                        sprintf(buf, "%s (%d)", me->p_weapons[0].sw_name,
                            me->p_weapons[0].sw_number);
                        pmessage(me->p_no, MINDIV, addrbuf,buf);
                        me->p_special = 0;
                    }
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;
        case 9:                 /* Nuclear warhead menu */
            switch (speed) {
            case 1:
            case 2:
            case 3:
            case 4:
                    if (speed > me->p_ship.s_maxarmies) {
                        warning("Not enough cargo room for that size nuke!");
                        break;
                    }
                    if (upgrade(0, me, "nuke inventory",
                        (float) (1 << (speed-1)), 0.0)) {
                        me->p_weapons[5+speed].sw_number += 1;
                        me->p_ship.s_maxarmies -= speed;
                        sndShipCap(); /* Need to update ship army capacity */
                        sprintf(buf, "%s (%d)", me->p_weapons[5+speed].sw_name,
                            me->p_weapons[5+speed].sw_number);
                        pmessage(me->p_no, MINDIV, addrbuf,buf);
                        me->p_special = 5+speed;
                    }
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;
        }
        return 0;
    }
    return 1;
}

void sturgeon_nplasmatorp(u_char course, int attributes)
{
  struct torp *k;
  register int i;
  struct torp *j;
  struct specialweapon *sw;
  struct planet *l;
  struct ship *myship;
  char buf[80];
  int destroyed, rnd;
  long random();

  myship = &me->p_ship;

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

  /* Normal plasmas and sturgeon SPECPLAS use similar checks */
  if(sw->sw_type == SPECPLAS)
  {
    if (weaponsallowed[WP_PLASMA]==0) {
      new_warning(17,"Plasmas haven't been invented yet.");
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
    if (me->p_flags & PFREPAIR) {
      new_warning(23,"We cannot fire while our vessel is undergoing repairs.");
      return;
    }
    if ((me->p_cloakphase) && (me->p_ship.s_type != ATT)) {
      if (!me->p_upgradelist[UPG_FIRECLOAK]) {
        new_warning(UNDEF,"We don't have the proper upgrade to fire while cloaked, captain!");
        return;
      }
    }
    for (i = me->p_no * MAXPLASMA, k = firstPlasmaOf(me);
         i < me->p_no * MAXPLASMA + MAXPLASMA; i++, k++) {
      if (k->t_status == TFREE)
        break;  /* found.  what if not found??? */
    }
    if (k > lastPlasmaOf(me))  {
      new_warning(UNDEF,"We are out of plasma, for the moment, Captain!");
      return;
    }
#if 0
    /* Pseudo code for run-time debugging, if someone wants it. */
    if (k > lastPlasmaOf(me))
      error();
#endif

    if (sw->sw_number > 0)
      sw->sw_number--;
    me->p_nplasmatorp++;
    me->p_wtemp += (sw->sw_damage * 1.5 + 10); /* Heat weapons */
    if (sw->sw_number < 0)
      sprintf(buf, "Firing %s", sw->sw_name);
    else
      sprintf(buf, "Firing %s (%d left)", sw->sw_name, sw->sw_number);
    new_warning(UNDEF,buf);

    k->t_no = i;
    k->t_turns = sw->sw_turns;
    k->t_damage = sw->sw_damage;
    k->t_gspeed = (attributes & TVECTOR) ? torpGetVectorSpeed(me->p_dir, me->p_speed, course, sw->sw_speed)
                  : sw->sw_speed * WARP1;
    k->t_fuse = sw->sw_fuse * T_FUSE_SCALE;
    k->t_pldamage = 0;
    k->t_status = TMOVE;
    k->t_type = TPLASMA;
    k->t_attribute = attributes;
    k->t_owner = me->p_no;
    t_x_y_set(k, me->p_x, me->p_y);
    k->t_dir    = ((myship->s_type == STARBASE) ||
                  (myship->s_type == ATT)) ? course : me->p_dir;
    k->t_war    = me->p_war;
    k->t_team   = me->p_team;
    k->t_whodet = NODET;
#if 0
    k->t_speed = k->t_gspeed / WARP1;
#endif
  } /* End SPECPLAS */

  switch (sw->sw_type) {
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
      t_x_y_set(k, l->pl_x, l->pl_y);
      k->t_dir = k->t_gspeed = k->t_war = 0;
      k->t_damage = 1;
      k->t_fuse = 100 * T_FUSE_SCALE;
      k->t_turns = 0;
      k->t_plbombed = l->pl_no;
      k->t_pldamage = 0;

      destroyed = sw->sw_damage;
      rnd = random() % 100;
      if (rnd < 5) {                                  /* uh oh... */
        new_warning(UNDEF,"Nuke explodes in bomb bay!");
        destroyed = 0;
        t_x_y_set(k, me->p_x, me->p_y);
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
      sndShipCap();                   /* Need to update ship army capacity */
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
      t_x_y_set(j, me->p_x, me->p_y);
      j->t_dir = course;
      j->t_damage = sw->sw_damage;
      j->t_gspeed = sw->sw_speed * WARP1;
      j->t_war = me->p_war;
      j->t_fuse = (sw->sw_fuse + (random() % 20)) * T_FUSE_SCALE;
      j->t_turns = sw->sw_turns;
      break;
    default:
      break;
  }

#ifdef LTD_STATS

  /* At this point, a plasma was fired */
  ltd_update_plasma_fired(me);

#endif

}

#endif

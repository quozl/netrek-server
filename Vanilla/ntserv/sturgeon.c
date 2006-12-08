#include <stdio.h>
#include <math.h>
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
            case 5: pmessage(me->p_no, MINDIV, addrbuf,"Special: 0=abort, 1=plasmas, 2=nukes, 3=drones, 4=mines, 5=inventory");
                    me->p_upgrading = 5;
                    break;
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
                        sturgeon_apply_upgrade(100, me, 0); /* Dummy upgrade just to send ship cap */
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

#endif

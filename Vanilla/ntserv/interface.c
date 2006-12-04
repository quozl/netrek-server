/*
 * interface.c
 *
 * This file will include all the interfaces between the input routines
 *  and the daemon.  They should be useful for writing robots and the
 *  like 
 */
#include "copyright.h"

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "proto.h"

/* file scope prototypes */
static void sendwarn(char *string, int atwar, int team);

#ifdef STURGEON
int upgrade(int type, struct player *me, char *item, double base, double adder)
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
            unapply_upgrade(type, me, 1);
            me->p_upgrades -= kills_gained;
            me->p_undo_upgrade = 0;
            return(0);
        }
        else {
            new_warning(UNDEF,"Undo upgrade failed - you don't have that upgrade.");
            return(0);
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
                return(0);
            }
            else if ((me->p_upgrades + kills_req) > sturgeon_maxupgrades) {
                sprintf(buf, "That would put you over the upgrade limit,  try buying something less expensive.");
                new_warning(UNDEF,buf);
                return(0);
            }
        }
    }
    /* Only actual kills can buy temporary items */
    else {
        if (me->p_kills < kills_req) {
            sprintf(buf, "You need %0.2f kills to increase your ship's %s.",
                    kills_req, item);
            new_warning(UNDEF,buf);
            return(0);
        }
        /* Limit on shield overcharge */
        if (strstr(item, "shields")) {
            long shieldboost;

            shieldboost = (100 * me->p_shield)/(me->p_ship.s_maxshield);
            if (shieldboost >= 200) {
                sprintf(buf, "Your shields (%ld percent normal) cannot be overcharged any more.", shieldboost);
                new_warning(UNDEF,buf);
                return(0);
            }
        }
    }
    me->p_flags |= PFREFITTING;
    if (type) {
        me->p_upgrades += kills_req;
        me->p_upgradelist[type]++;
    }
    /* Decrement any rank credit first, for actual upgrades */
    if (type > 0 && me->p_rankcredit >= kills_req)
        me->p_rankcredit -= kills_req;
    else if (type > 0 && me->p_rankcredit > 0.0) {
        me->p_kills -= (kills_req - me->p_rankcredit);
        me->p_rankcredit = 0.0;
    }
    else
        me->p_kills -= kills_req;

    rdelay = me->p_updates + (kills_req * 10);
    if (type < UPG_OFFSET)
        sprintf(buf, "Ship's %s increased.", item);
    else /* 1 time upgrades */
        sprintf(buf, "You have gained the %s upgrade!", item);
    new_warning(UNDEF,buf);
    return(1);
}

/* Function to actually apply all upgrades */
void apply_upgrade(int type, struct player *j, int multiplier)
{
    int i = multiplier;

    switch(type)
    {
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
void unapply_upgrade(int type, struct player *j, int multiplier)
{
    int i = multiplier;

    switch(type)
    {
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
#endif

void set_speed(int speed)
{

#ifdef STURGEON
    struct ship tmpship;
    char buf[100];
    char addrbuf[10];
    int i;

    if (sturgeon && me->p_upgrading)
    {
        if (speed == 0) {
            new_warning(UNDEF,"Upgrade aborted");
            me->p_flags &= ~(PFREFIT);
            me->p_upgrading = 0;
            return;
        }
        sprintf(addrbuf,"UPG->%c%c ", teamlet[me->p_team], shipnos[me->p_no]);
        getship(&tmpship, me->p_ship.s_type);
        switch(me->p_upgrading)
        {
        case 1:                 /* first level menu */
            switch(speed)
            {
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
            switch(speed)
            {
            case 1: if (upgrade(UPG_PERMSHIELD, me, "shield max", 0.0, 0.0))
                        apply_upgrade(UPG_PERMSHIELD, me, 1);
                    break;
            case 2: if (upgrade(UPG_TEMPSHIELD, me, "shields (temporarily)", 1.0, 0.0))
                        apply_upgrade(UPG_TEMPSHIELD, me, 1);
                    break;
            case 3: if (upgrade(UPG_HULL, me, "hull max", 0.0, 0.0))
                        apply_upgrade(UPG_HULL, me, 1);
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;
        case 3:                 /* engine upgrade menu */
            switch(speed)
            {
            case 1: if (upgrade(UPG_FUEL, me, "fuel capacity", 0.0, 0.0))
                        apply_upgrade(UPG_FUEL, me, 1);
                    break;
            case 2: if (upgrade(UPG_RECHARGE, me, "fuel collector efficiency", 0.0, 0.0))
                        apply_upgrade(UPG_RECHARGE, me, 1);
                    break;
            case 3: if (upgrade(UPG_MAXWARP, me, "maximum warp speed", 0.0, 0.0))
                        apply_upgrade(UPG_MAXWARP, me, 1);
                    break;
            case 4: if (upgrade(UPG_ACCEL, me, "acceleration rate", 0.0, 0.0))
                        apply_upgrade(UPG_ACCEL, me, 1);
                    break;
            case 5: if (upgrade(UPG_DECEL, me, "deceleration rate", 0.0, 0.0))
                         apply_upgrade(UPG_DECEL, me, 1);
                    break;
            case 6: if (upgrade(UPG_ENGCOOL, me, "engine cooling rate", 0.0, 0.0))
                         apply_upgrade(UPG_ENGCOOL, me, 1);
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;
        case 4:                 /* weapon upgrade menu */
            switch(speed)
            {
            case 1: if (upgrade(UPG_PHASER, me, "phaser damage", 0.0, 0.0))
                        apply_upgrade(UPG_PHASER, me, 1);
                    break;
            case 2: if (upgrade(UPG_TORPSPEED, me, "photon speed", 0.0, 0.0))
                        apply_upgrade(UPG_TORPSPEED, me, 1);
                    break;
            case 3: if (upgrade(UPG_TORPFUSE, me, "photon fuse", 0.0, 0.0))
                        apply_upgrade(UPG_TORPFUSE, me, 1);
                    break;
            case 4: if (upgrade(UPG_WPNCOOL, me, "weapon cooling rate", 0.0, 0.0))
                        apply_upgrade(UPG_WPNCOOL, me, 1);
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;
        case 5:                 /* special weapons menu */
            switch(speed)
            {
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
            switch(speed)
            {
            case 1:
                    if (me->p_upgradelist[UPG_FIRECLOAK] && !me->p_undo_upgrade) {
                        pmessage(me->p_no, MINDIV, addrbuf,"You can already fire while cloaked.");
                        break;
                    }
                    if (upgrade(UPG_FIRECLOAK, me, "fire while cloak", 0.0, 0.0))
                        apply_upgrade(UPG_FIRECLOAK, me, 1);
                    break;
            case 2:
                    if (me->p_upgradelist[UPG_DETDMG] && !me->p_undo_upgrade) {
                        pmessage(me->p_no, MINDIV, addrbuf,"You can already det own torps for damage.");
                        break;
                    }
                    if (upgrade(UPG_DETDMG, me, "det own torps for damage", 0.0, 0.0))
                        apply_upgrade(UPG_DETDMG, me, 1);
                    break;
            default:
                    me->p_flags &= ~(PFREFIT);
                    me->p_upgrading = 0;
            }
            break;

        case 7:                 /* miscellaneous systems menu */
            switch(speed)
            {
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
                    apply_upgrade(UPG_TPSTR, me, 1);
                break;
            case 3:
                if (upgrade(UPG_TPRANGE, me, "tractor/pressor range", 0.0, 0.0))
                    apply_upgrade(UPG_TPRANGE, me, 1);
                break;
            case 4:
                if (upgrade(UPG_REPAIR, me, "damage control efficiency", 0.0, 0.0))
                    apply_upgrade(UPG_REPAIR, me, 1);
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
            switch(speed)
            {
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
            switch(speed)
            {
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
                        apply_upgrade(100, me, 0); /* Dummy upgrade just to send ship cap */
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
        return;
    }
#endif

    if (speed > me->p_ship.s_maxspeed) {
	me->p_desspeed = me->p_ship.s_maxspeed;
    } else if (speed < 0) {
	speed=0;
    }
    me->p_desspeed = speed;
    bay_release(me);
    me->p_flags &= ~(PFREPAIR | PFBOMB | PFORBIT | PFBEAMUP | PFBEAMDOWN);
}

void set_course(u_char dir)
{
    me->p_desdir = dir;
    bay_release(me);
    me->p_flags &= ~(PFBOMB | PFORBIT | PFBEAMUP | PFBEAMDOWN);
}

void shield_up(void)
{
    me->p_flags |= PFSHIELD;
    me->p_flags &= ~(PFBOMB | PFREPAIR | PFBEAMUP | PFBEAMDOWN);
}

void shield_down(void)
{
    me->p_flags &= ~PFSHIELD;
}

void shield_tog(void)
{
    me->p_flags ^= PFSHIELD;
    me->p_flags &= ~(PFBOMB | PFREPAIR | PFBEAMUP | PFBEAMDOWN);
}

void bomb_planet(void)
{
    static int bombsOutOfTmode = 0; /* confirm bomb out of t-mode 4/6/92 TC */
    int owner;			/* temporary variable 4/6/92 TC */

    if (!(me->p_flags & PFORBIT)) {
        new_warning(39,"Must be orbiting to bomb");
	return;
    }

    /* no bombing of own armies, friendlies 4/6/92 TC */

    owner = planets[me->p_planet].pl_owner;

    if (me->p_team == owner) {
        new_warning(40,"Can't bomb your own armies.  Have you been reading Catch-22 again?");
	return;
    }
    if (!(me->p_war & owner)) {
        new_warning(41,"Must declare war first (no Pearl Harbor syndrome allowed here).");
	return;
    }

    if(restrict_bomb
#ifdef PRETSERVER
            /* if this is the pre-T entertainment we don't require confirmation */
            && !bot_in_game 
#endif
        ) {
        if (!status->tourn){
            new_warning(UNDEF,"You may not bomb out of T-mode.");
          return;
        }
    }

    if ((!status->tourn) && (bombsOutOfTmode == 0)
#ifdef PRETSERVER
            /* if this is the pre-T entertainment we don't require confirmation */
            && !bot_in_game 
#endif
        ) {
        new_warning(42,"Bomb out of T-mode?  Please verify your order to bomb.");
        bombsOutOfTmode++;
        return;
    }

#ifdef PRETSERVER
    if(bot_in_game && realNumShips(owner) == 0) {
        new_warning(UNDEF,"You may not bomb 3rd and 4th space planets.");
        return;
    }
#endif

    if(no_unwarring_bombing) {
/* Added ability to take back your own planets from 3rd team 11-15-93 ATH */
        if ((status->tourn && realNumShips(owner) < tournplayers) 
          && !(me->p_team & planets[me->p_planet].pl_flags)) {
            new_warning(UNDEF,"You may not bomb 3rd and 4th space planets.");
            return;
         }
    }

    if(! restrict_bomb
#ifdef PRETSERVER
            /* if this is the pre-T entertainment we don't require confirmation */
            && !bot_in_game 
#endif
        )
    {
        if ((!status->tourn) && (bombsOutOfTmode == 1)) {
            new_warning(43,"Hoser!");
            bombsOutOfTmode++;
        }
    }

    if (status->tourn) bombsOutOfTmode = 0;

    me->p_flags |= PFBOMB;
    me->p_flags &= ~(PFSHIELD | PFREPAIR | PFBEAMUP | PFBEAMDOWN);
}

void beam_up(void)
{
    if (!(me->p_flags & (PFORBIT | PFDOCK))) {
        new_warning(44,"Must be orbiting or docked to beam up.");
	return;
    }
    if (me->p_flags & PFORBIT) {
	if (me->p_team != planets[me->p_planet].pl_owner) {
            new_warning(45,"Those aren't our men.");
	    return;
	}
    } else if (me->p_flags & PFDOCK) {
	if (me->p_team != players[me->p_dock_with].p_team) {
            new_warning(46,"Comm Officer: We're not authorized to beam foriegn troops on board!");
	    return;
	}
    }
    me->p_flags |= PFBEAMUP;
    me->p_flags &= ~(PFSHIELD | PFREPAIR | PFBOMB | PFBEAMDOWN);
}

void beam_down(void)
{
    int owner;

    if (!(me->p_flags & (PFORBIT | PFDOCK))) {
        new_warning(47, "Must be orbiting or docked to beam down.");
        return;
    }

#ifdef PRETSERVER
    if(pre_t_mode && me->p_flags & PFORBIT) {
        owner = planets[me->p_planet].pl_owner;
        if(bot_in_game && realNumShips(owner) == 0 && owner != NOBODY) {
            new_warning(UNDEF,"You may not drop on 3rd and 4th space planets. Sorry Bill.");
            return;
        }
    }
#endif

    if (me->p_flags & PFDOCK) {
        if (me->p_team != players[me->p_dock_with].p_team) {
            new_warning(48,"Comm Officer: Starbase refuses permission to beam our troops over.");
	    return;
        }
    }
    me->p_flags |= PFBEAMDOWN;
    me->p_flags &= ~(PFSHIELD | PFREPAIR | PFBOMB | PFBEAMUP);
}

void repair(void)
{
#ifndef NEW_ETEMP
    if ((me->p_flags & PFENG) && (me->p_speed != 0)) {
	new_warning(UNDEF,"Sorry, can't repair with melted engines while moving.");
    } else 
#endif
    {
	me->p_desspeed = 0;
	me->p_flags |= PFREPAIR;
	me->p_flags &= ~(PFSHIELD | PFBOMB | PFBEAMUP | PFBEAMDOWN | PFPLOCK | PFPLLOCK);
    }
}

void repair_off(void)
{
    me->p_flags &= ~PFREPAIR;
}

void repeat_message(void)
{
    if (++lastm == MAXMESSAGE) ;
	lastm = 0;
}

void cloak(void)
{
    me->p_flags ^= PFCLOAK;
    me->p_flags &= ~(PFTRACT | PFPRESS);
}

void cloak_on(void)
{
    me->p_flags |= PFCLOAK;
    me->p_flags &= ~(PFTRACT | PFPRESS);
}

void cloak_off(void)
{
    me->p_flags &= ~PFCLOAK;
}

void lock_planet(int planet)
{
    if (planet<0 || planet>=MAXPLANETS) return;

    me->p_flags |= PFPLLOCK;
    me->p_flags &= ~(PFPLOCK|PFORBIT|PFBEAMUP|PFBEAMDOWN|PFBOMB);
    me->p_planet = planet;
    if(send_short) {
                swarning(LOCKPLANET_TEXT, (u_char) planet, 0);
    }
    else {
    new_warning(UNDEF,"Locking onto %s", planets[planet].pl_name);
     }
}

void lock_player(int player)
{
    if (player<0 || player>=MAXPLAYER) return;
    if (players[player].p_status != PALIVE) return;
    if (players[player].p_flags & PFCLOAK && !Observer) return;

    me->p_flags |= PFPLOCK;
    me->p_flags &= ~(PFPLLOCK|PFORBIT|PFBEAMUP|PFBEAMDOWN|PFBOMB);
    me->p_playerl = player;

    /* notify player docking perm status of own SB when locking 7/19/92 TC */

    if ((players[player].p_team == me->p_team) &&
	(players[player].p_ship.s_type == STARBASE) &&
	(me->p_candock)) 
        {
        if(send_short) {
                swarning(SBLOCKMYTEAM,player,0);
                return;
        }
        else
	new_warning(UNDEF, "Locking onto %s (%c%c) (docking is %s)",
		players[player].p_name,
		teamlet[players[player].p_team],
		shipnos[players[player].p_no],
		(players[player].p_flags & PFDOCKOK) ? "enabled" : "disabled");
        }
    else
        {
        if(send_short) {
                swarning(SBLOCKSTRANGER,player,0);
                return;
        }
        else
	new_warning(UNDEF, "Locking onto %s (%c%c)",
		players[player].p_name,
		teamlet[players[player].p_team],
		shipnos[players[player].p_no]);
        }
}

void tractor_player(int player)
{
    struct player *victim;

    if (weaponsallowed[WP_TRACTOR]==0) {
	return;
    }
    if ((player < 0) || (player > MAXPLAYER)) {	/* out of bounds = cancel */
	me->p_flags &= ~(PFTRACT | PFPRESS);
	return;
    }
    if (me->p_flags & PFCLOAK) {
	return;
    }
    if (player==me->p_no) return;
    victim= &players[player];
    if (victim->p_flags & PFCLOAK) return;
    if (hypot((double) me->p_x-victim->p_x,
	    (double) me->p_y-victim->p_y) < 
	    ((double) TRACTDIST) * me->p_ship.s_tractrng) {
	bay_release(victim);
	bay_release(me);
	victim->p_flags &= ~PFORBIT;
	me->p_flags &= ~PFORBIT;
	me->p_tractor = player;
	me->p_flags |= PFTRACT;
    } else {			/* out of range */
    }
}

void pressor_player(int player)
{
    int target;
    struct player *victim;

    if (weaponsallowed[WP_TRACTOR]==0) {
        new_warning(0,"Tractor beams haven't been invented yet.");
	return;
    }

    target=player;

    if ((target < 0) || (target > MAXPLAYER)) {	/* out of bounds = cancel */
        me->p_flags &= ~(PFTRACT | PFPRESS);
        return;
    }
    if (me->p_flags & PFPRESS) return; /* bug fix: moved down 5/1/92 TC */

    if (me->p_flags & PFCLOAK) {
        new_warning(1,"Weapons's Officer:  Cannot tractor while cloaked, sir!");
	return;
    }
    
    victim= &players[target];
    if (victim->p_flags & PFCLOAK) return;
    
    if (hypot((double) me->p_x-victim->p_x,
	      (double) me->p_y-victim->p_y) < 
	((double) TRACTDIST) * me->p_ship.s_tractrng) {
	bay_release(victim);
	bay_release(me);
	victim->p_flags &= ~(PFORBIT | PFDOCK);
	me->p_flags &= ~(PFORBIT | PFDOCK);
	me->p_tractor = target;
	me->p_flags |= (PFTRACT | PFPRESS);
    } else {
        new_warning(2,"Weapon's Officer:  Vessel is out of range of our tractor beam.");
    }
    
}

void declare_war(int mask, int refitdelay)
{
    int changes;
    int i;

    /* indi are forced to be at peace with everyone */

    if( !(me->p_flags & PFROBOT) && (me->p_team == NOBODY) ) {
      me->p_war = NOBODY;
      me->p_hostile = NOBODY;
      me->p_swar = NOBODY;
      return;
    }

    mask &= ALLTEAM;
    mask &= ~me->p_team;
    mask |= me->p_swar;
    changes=mask ^ me->p_hostile;
    if (changes==0) return;

    if (changes & FED) {
	sendwarn("Federation", mask & FED, FED);
    }
    if (changes & ROM) {
	sendwarn("Romulans", mask & ROM, ROM);
    }
    if (changes & KLI) {
	sendwarn("Klingons", mask & KLI, KLI);
    }
    if (changes & ORI) {
	sendwarn("Orions", mask & ORI, ORI);
    }
    if (me->p_flags & PFDOCK) {
	if (players[me->p_dock_with].p_team & mask) {
	    /* release ship from starbase that is now hostile */
	    bay_release(me);
	}
    }
    if (me->p_ship.s_type == STARBASE) {
        for(i=0; i<NUMBAYS; i++) {
	    if (me->p_bays[i] == VACANT)
                continue;
	    if (mask & players[me->p_bays[i]].p_team) {
	        /* release docked ships that are now hostile */
	        bay_release(&players[me->p_bays[i]]);
	    }
	}
    }

    if (refitdelay && (mask & ~me->p_hostile)) {
	me->p_flags |= PFWAR;
	delay = me->p_updates + 100;
        new_warning(49,"Pausing ten seconds to re-program battle computers.");
    }
    me->p_hostile = mask;
    me->p_war = (me->p_swar | me->p_hostile);
}

static void sendwarn(char *string, int atwar, int team)
{
    char addrbuf[10];

    (void) sprintf(addrbuf, " %c%c->%-3s",
        teamlet[me->p_team],
        shipnos[me->p_no],
        teamshort[team]);

    if (atwar) {
	pmessage2(team, MTEAM, addrbuf, me->p_no,
            "%s (%c%c) declaring war on the %s",
            me->p_name,
            teamlet[me->p_team],
            shipnos[me->p_no],
            string);
    }
    else {
	pmessage2(team, MTEAM, addrbuf, me->p_no,
            "%s (%c%c) declaring peace with the %s",
            me->p_name,
            teamlet[me->p_team],
            shipnos[me->p_no],
            string);
    }
}

void do_refit(int type)
{	
    int i=0;
#ifdef STURGEON
    char buf[80];
    char addrbuf[80];
#endif

    if (type<0 || type>ATT) return;
    if (me->p_flags & PFORBIT) {
	if (!(planets[me->p_planet].pl_flags & PLHOME)) {
            new_warning(50,"You must orbit your HOME planet to apply for command reassignment!");
	    return;
	} else {
	    if (!(planets[me->p_planet].pl_flags & me->p_team)) {
                new_warning(51,"You must orbit your home planet to apply for command reassignment!");
		return;
	    }
	} /* if (PLHOME) */
    } else if (me->p_flags & PFDOCK) {
	if (type == STARBASE) {
            new_warning(52,"Can only refit to starbase on your home planet.");
	    return;
	}
	if (players[me->p_dock_with].p_team != me->p_team) {
            new_warning(53,"You must dock YOUR starbase to apply for command reassignment!");
	    return;
	}
    } else {
        new_warning(54,"Must orbit home planet or dock your starbase to apply for command reassignment!");
	return;
    } 

    if ((me->p_damage> ((float)me->p_ship.s_maxdamage)*.75) || 
	    (me->p_shield < ((float)me->p_ship.s_maxshield)*.75) ||
	    (me->p_fuel < ((float)me->p_ship.s_maxfuel)*.75)) {
        new_warning(55,"Central Command refuses to accept a ship in this condition!");
	return;
    }

    if ((me->p_armies > 0)) {
        new_warning(56,"You must beam your armies down before moving to your new ship");
	return;
    }

#ifdef STURGEON
    if (sturgeon && (type == me->p_ship.s_type) && upgradeable) {
        /* No upgrades for starbases */
        if (type != STARBASE) {
          sprintf(addrbuf,"UPG->%c%c ", teamlet[me->p_team], shipnos[me->p_no]);
          pmessage(me->p_no, MINDIV, addrbuf,"Upgrade: 0=abort, 1=normal refit, 2=shields/hull, 3=engines");
          pmessage(me->p_no, MINDIV, addrbuf,"         4=weapons, 5=special weapons, 6=one time upgrades, 7=misc");
          new_warning(UNDEF,"Please make an upgrade selection");
          me->p_upgrading = 1;
          return;
        }
    }
#endif

    if (shipsallowed[type]==0) {
        new_warning(57,"That ship hasn't been designed yet.");
	return;
    }

    if (type == DESTROYER) {
        if (me->p_stats.st_rank < ddrank) {
            new_warning(UNDEF,"You need a rank of %s or higher to command a destroyer!", ranks[ddrank].name);
            return;
        }
    }
    if (type == SGALAXY) {
        if (me->p_stats.st_rank < garank) {
            new_warning(UNDEF,"You need a rank of %s or higher to command a galaxy class ship!", ranks[garank].name);
            return;
        }
    }
    if (type == STARBASE) {
	if (me->p_stats.st_rank < sbrank) {
            if(send_short){
                swarning(SBRANK_TEXT,sbrank,0);
            }
            else {

	    new_warning(UNDEF,"You need a rank of %s or higher to command a starbase!", ranks[sbrank].name);
            }
	    return;
	}
    }
    if (type == STARBASE && chaos) {
	int num_bases = 0;
	for (i=0; i<MAXPLAYER; i++) 
     	    if ((me->p_no != i) && 
		(players[i].p_status == PALIVE) && 
		(players[i].p_team == me->p_team))
		if (players[i].p_ship.s_type == STARBASE) {
		  num_bases++;
		}
	if (num_bases >= max_chaos_bases) {
	  new_warning(UNDEF,"Your side already has %d starbase%s",max_chaos_bases,(max_chaos_bases>1) ? "s!":"!");
	  return;
	}
    }
    else if (type == STARBASE && !chaos) {
	for (i=0; i<MAXPLAYER; i++) 
	    if ((me->p_no != i) && 
		(players[i].p_status == PALIVE) && 
		(players[i].p_team == me->p_team))
		if (players[i].p_ship.s_type == STARBASE) {
                    new_warning(58,"Your side already has a starbase!");
		    return;
		}
    }
    if (type == STARBASE && !chaos && !topgun) {
	if (realNumShips(me->p_team) < 4) {
            if(send_short){
                swarning(TEXTE,59,0);
            }
            else
	    new_warning(UNDEF,"Your team is not capable of defending such an expensive ship");
	    return;
	}
    }
    if (type == STARBASE && !chaos && !topgun) { /* change TC 12/10/90 */
	if (teams[me->p_team].s_turns > 0) {
	  new_warning(UNDEF,"Not constructed yet. %d minutes required for completion",teams[me->p_team].s_turns);
	    return;
	}
    }
    if (type == STARBASE && numPlanets(me->p_team) < sbplanets && !topgun) {
        new_warning(61,"Your team's stuggling economy cannot support such an expenditure!");
	return;
    }

    if ((me->p_ship.s_type == STARBASE)/* && (type != STARBASE)*/) {
	/* Reset kills to 0.0 */
	me->p_kills=0;
	/* bump all docked ships */
	bay_release_all(me);
	me->p_flags |= PFDOCKOK;
    }	

    
    getship(&(me->p_ship), type);

#ifdef STURGEON
    if (sturgeon) {
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
        }
        else {
            /* Now we need to go through the upgrade list and reapply all upgrades
               that are left, as default ship settings have been reset */
            for (i = 1; i < NUMUPGRADES; i++) {
              if (me->p_upgradelist[i] > 0)
                apply_upgrade(i, me, me->p_upgradelist[i]);
            }
        }
        switch(type)
        {
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
#endif

    /* Notify client of new ship stats, if necessary */
    sndShipCap();
    if (type != STARBASE && me->p_kills < plkills) {
	me->p_ship.s_plasmacost = -1;
    }

    if (type != STARBASE && type != ATT && (
#ifdef STURGEON  /* force plasmas -> upgrade feature */
        !sturgeon &&
#endif
        me->p_kills < plkills))
        me->p_ship.s_plasmacost = -1;
    me->p_shield = me->p_ship.s_maxshield;
    me->p_damage = 0;
    me->p_fuel = me->p_ship.s_maxfuel;
    me->p_wtemp = 0;
    me->p_wtime = 0;
    me->p_etemp = 0;
    me->p_etime = 0;
    me->p_ship.s_type = type;
    if (type == STARBASE) {
	bay_init(me);
	me->p_flags |= PFDOCKOK;
    }

    me->p_flags &= ~(PFREFIT|PFWEP|PFENG);
    me->p_flags |= PFREFITTING;
    rdelay = me->p_updates + 50;

    new_warning(62,"You are being transported to your new vessel .... ");
}

int numPlanets(int owner)
{
    int i, num=0;
    struct planet *p;

    for (i=0, p=planets; i<MAXPLANETS; i++, p++) {
        if (p->pl_owner==owner)  {
            num++;
        }
    }
    return(num);
}

int sndShipCap(void)
{
    struct ship_cap_spacket ShipFoo;

#ifndef ROBOT
    if ((F_ship_cap && !sent_ship[me->p_ship.s_type])
#ifdef STURGEON
        || sturgeon
#endif
        ) {
        sent_ship[me->p_ship.s_type] = 1;
        ShipFoo.type = SP_SHIP_CAP;
        ShipFoo.s_type = htons(me->p_ship.s_type);
        ShipFoo.operation = 0;
        ShipFoo.s_torpspeed = htons(me->p_ship.s_torpspeed);
        ShipFoo.s_maxfuel = htonl(me->p_ship.s_maxfuel);
        ShipFoo.s_maxspeed = htonl(me->p_ship.s_maxspeed);
        ShipFoo.s_maxshield = htonl(me->p_ship.s_maxshield);
        ShipFoo.s_maxdamage = htonl(me->p_ship.s_maxdamage);
        ShipFoo.s_maxwpntemp = htonl(me->p_ship.s_maxwpntemp);
        ShipFoo.s_maxegntemp = htonl(me->p_ship.s_maxegntemp);
        ShipFoo.s_width = htons(me->p_ship.s_width);
        ShipFoo.s_height = htons(me->p_ship.s_height);
        ShipFoo.s_maxarmies = htons(me->p_ship.s_maxarmies);
        ShipFoo.s_letter = "sdcbaog*"[me->p_ship.s_type];
        ShipFoo.s_desig1 = shiptypes[me->p_ship.s_type][0];
        ShipFoo.s_desig2 = shiptypes[me->p_ship.s_type][1];
        ShipFoo.s_phaserrange = htons(me->p_ship.s_phaserdamage);
        ShipFoo.s_bitmap = htons(me->p_ship.s_type);
        strcpy(ShipFoo.s_name,shipnames[me->p_ship.s_type]);
        sendClientPacket((CVOID) &ShipFoo);
        return (1);
    }
#endif
    return (0);
}

/*
 * coup.c
 */
#include <stdio.h>
#include "copyright.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"

/* throw a coup */

void coup(void)
{
    register int i;
    register struct planet *l;

#ifdef STURGEON
    /* Coup key (rarely used) doubles as toggle key to switch between various
       special weapons in inventory */
    l = &planets[me->p_planet];
    /* Make sure player not actually in a position to coup */
    if (sturgeon && (!(me->p_flags & PFORBIT) ||
        !(l->pl_flags & PLHOME) ||
        (l->pl_owner == me->p_team) ||
        !(l->pl_flags & me->p_team)))
    {
        char buf[80];
        int t = me->p_special;
        if (t < 0) {
            new_warning(UNDEF,"This ship is not equipped with special weapons.");
            return;
        }
        for (t = (t+1) % NUMSPECIAL; me->p_weapons[t].sw_number == 0 &&
             t != me->p_special; t = (t+1) % NUMSPECIAL)
            ;

        if ((t == me->p_special) && (me->p_weapons[t].sw_number == 0)) {
            new_warning(UNDEF, "We have exhausted all of our special weapons, Captain.");
            me->p_special = -1;
        }
        else {
            if (me->p_weapons[t].sw_number < 0)  /* infinite */
                sprintf(buf, "Special Weapon: %s", me->p_weapons[t].sw_name);
            else
                sprintf(buf, "Special Weapon: %s (%d left)",
                    me->p_weapons[t].sw_name, me->p_weapons[t].sw_number);

            new_warning(UNDEF,buf);
            me->p_special = t;
        }
        return;
    }
#endif

    if (me->p_kills < 1.0) {
        new_warning(3,"You must have one kill to throw a coup");
	return;
    }
    if (!(me->p_flags & PFORBIT)) {
        new_warning(4,"You must orbit your home planet to throw a coup");
	return;
    }
    for (i = 0, l = &planets[i]; i < MAXPLANETS; i++, l++) {
	if ((l->pl_owner == me->p_team) && (l->pl_armies > 0)) {
            new_warning(5,"You already own a planet!!!");
	    return;
	}
    }
    l = &planets[me->p_planet];

    if ((!(l->pl_flags & PLHOME)) || ((l->pl_flags & ALLTEAM) != me->p_team)) {
        new_warning(6, "You must orbit your home planet to throw a coup");
	return;
    }

    if (l->pl_armies > 4) {
        new_warning(7,"Too many armies on planet to throw a coup");
	return;
    }

    if (l->pl_couptime > 0) {
        new_warning(UNDEF,"Can't throw a coup for %d more minute%s",l->pl_couptime,(l->pl_couptime>1)?"s!":"!");
	return;
    }

    if (l->pl_flags & PLCOUP) { /* Avoid race conditions */
	return;
    }

    /* the cases are now met.  We can have a coup. */

    if (!(l->pl_flags & PLCOUP)) {       /* avoid couping same thing twice */
	l->pl_flags |= PLCOUP;
	me->p_planets++;
	me->p_genoplanets++;

#ifndef LTD_STATS	/* LTD stats do not support planet credits for coups */

	if (status->tourn) {

#ifdef NEW_CREDIT
	    me->p_stats.st_tplanets+=3;
	    status->planets+=3;
#else
	    me->p_stats.st_tplanets++;
	    status->planets++;
#endif
	} else {
#ifdef NEW_CREDIT
	    me->p_stats.st_planets+=3;
#else
	    me->p_stats.st_planets++;
#endif
	}

#endif /* LTD_STATS */
    }
}

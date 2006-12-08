/*
 * coup.c
 */
#include <stdio.h>
#include "copyright.h"
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "sturgeon.h"

/* throw a coup */

void coup(void)
{
    int i;
    struct planet *l;

#ifdef STURGEON
    if (sturgeon && !sturgeon_hook_coup()) return;
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

/* 
 * slotmaint.c
 *
 * Michael Kantner, October 1994
 *
 */
#include "copyright2.h"

#include <stdio.h>
#include <sys/types.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

/* 
 *  Free a slot that was allocated with pickslot.
 *  This leaves the slot in good shape for the next user.
 *  returns:  1 if successful,
 *           -1 if player is NULL (i.e. me was not initialized)
 *           -2 if slot already PFREE
 */
int freeslot(struct player *who)
{
    int retvalue = 1;

    if (who == NULL) return (-1);
    if (who->p_status == PFREE){
	ERROR(1,("freeslot: Trying to free a PFREE slot\n"));
	retvalue = -2;
	/* Since I should have already incremented this, I decrement it
           now so that the later command does nothing.  A hack, but this
           code should never be executed. */
	queues[who->w_queue].free_slots--;
    }
    who->p_status      = PFREE;
    who->p_flags       = 0;
    who->p_ghostbuster = 0;
/*	Dangerous if the player get resurrected 
    MZERO(&who->p_stats, sizeof(struct stats));  
    who->p_stats.st_tticks=1;
*/
    queues[who->w_queue].free_slots++;
    MZERO(who->voting, sizeof(time_t) * PV_TOTAL);
    who->p_process     = 0;
    
    return retvalue;
}

/* 
 * Pick and allocate an empty slot.
 * returns: The slot number
 *          -1 if there is no free slot
 *          -2 if the queue is closed
 * It sets up the slot for use.  All generic code should be here.
 */
int pickslot(int w_queue)
{
    int i;
    int altslot;

    /* Ensure that the queue is open */
    if (!(queues[w_queue].q_flags & QU_OPEN)) return (-2);

    if (queues[w_queue].enter_sem-- < 1) return (-1);
    /* ASSERT: enter_sem <= 0, so no one else can enter */

    if (queues[w_queue].free_slots <= 0){  /* no free slots */
	queues[w_queue].enter_sem = 1;     /* give back control */
	return (-1);
    }

    for (altslot = i = queues[w_queue].alt_lowslot ?
         queues[w_queue].alt_lowslot : 0;; i++) {
        
        /* Allocate slots 8 - f for ROM before allocating lower slots */
        if (i == queues[w_queue].high_slot) {
            if (!altslot)
                break;
            i = altslot = 0;
            continue;
        }

        /* avoid allocating slot t unless flags say we can */
        if (i == 29 && !(queues[w_queue].q_flags & QU_TOK)) continue;

	if (players[i].p_status == PFREE) {     /* We have free slot */
	    int j;

	    players[i].p_status = POUTFIT;      /* possible race code */

	    /* Set up the player struct now */
	    players[i].p_no          = i;
	    players[i].p_team        = NOBODY;
	    players[i].p_ghostbuster = 0;
	    players[i].p_flags       = 0;
	    players[i].w_queue       = w_queue;
	    players[i].p_pos         = -1;
	    MZERO(&players[i].p_stats, sizeof(struct stats));  
#ifdef LTD_STATS

            ltd_reset(&players[i]);

#else

	    players[i].p_stats.st_tticks=1;

#endif /* LTD_STATS */

	    for (j=0; j<95; j++) {
		players[i].p_stats.st_keymap[j]=j+32;
	    }
	    players[i].p_stats.st_keymap[95]=0;
	    players[i].p_stats.st_flags=ST_INITIAL;
	    players[i].p_process     = 0;
	    MZERO(players[i].voting, sizeof(time_t) * PV_TOTAL);
#ifdef OBSERVERS
	    if (queues[w_queue].q_flags & QU_OBSERVER) Observer++;
#endif
	    /* Update the queue info */
	    queues[w_queue].free_slots--;
	    queues[w_queue].enter_sem = 1;      /* give back control */
	    return(i);
	}
    }

    queues[w_queue].enter_sem = 1;              /* give back control */
    return (-1);
}

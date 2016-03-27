/* 
 * slotmaint.c
 *
 * Michael Kantner, October 1994
 *
 */
#include "copyright2.h"
#include "config.h"

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "util.h"
#include "proto.h"
#include "slotmaint.h"

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
#ifdef LTD_STATS
    ltd_reset(who);
#else
    memset(&who->p_stats, 0, sizeof(struct stats));  
    who->p_stats.st_tticks=1;
#endif
    queues[who->w_queue].free_slots++;
    memset(who->voting, 0, sizeof(time_t) * PV_TOTAL);
    who->p_process     = 0;
    
    return retvalue;
}

/* check for reasons why a slot should not be allocated */
static int verboten(int w_queue, int i)
{
    /* avoid allocating slot t unless flags say we can */
    return (i == 29 && !(queues[w_queue].q_flags & QU_TOK));
}

static int pickslot_action(int w_queue, int i)
{
    int j;

    if (players[i].p_status != PFREE) return 0;
    if (verboten(w_queue, i)) return 0;

    lock_on(LOCK_PICKSLOT);
    if (players[i].p_status != PFREE) {
      lock_off(LOCK_PICKSLOT);
      return 0;
    }
    players[i].p_status = POUTFIT;      /* possible race code */
    lock_off(LOCK_PICKSLOT);

    /* Set up the player struct now */
    players[i].p_no          = i;
    players[i].p_team        = NOBODY;
    players[i].p_ghostbuster = 0;
    players[i].p_flags       = 0;
    players[i].w_queue       = w_queue;
    players[i].p_pos         = -1;
    memset(&players[i].p_stats, 0, sizeof(struct stats));
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
    memset(players[i].voting, 0, sizeof(time_t) * PV_TOTAL);
    if (queues[w_queue].q_flags & QU_OBSERVER) Observer++;
    /* Update the queue info */
    queues[w_queue].free_slots--;
    return 1;
}

/* scan a queue, calling an action function for each slot until the
action function returns non-zero, at which point the slot number is
returned, or -1 if no call to an action function returned non-zero */
static int scanqueue(int w_queue, int (*action)(int w_queue, int i))
{
    int i;

    /* try the slots from the prefered slot to the highest slot of the queue */
    /* (in case of INL trading, try slots 8 - f for away first) */
    if (queues[w_queue].prefer)
        for (i = queues[w_queue].prefer; i < queues[w_queue].high_slot; i++) {
            if ((*action)(w_queue, i)) return i;
        }

    /* try all the slots allocated to the queue */
    for (i = queues[w_queue].low_slot; i < queues[w_queue].high_slot; i++) {
        if ((*action)(w_queue, i)) return i;
    }

    return -1;
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

    /* Ensure that the queue is open */
    if (!(queues[w_queue].q_flags & QU_OPEN)) return -2;

    if (queues[w_queue].enter_sem-- < 1) return -1;
    /* ASSERT: enter_sem <= 0, so no one else can enter */

    if (queues[w_queue].free_slots <= 0) {  /* no free slots */
        queues[w_queue].enter_sem = 1;      /* give back control */
        return -1;
    }

    i = scanqueue(w_queue, pickslot_action);
    queues[w_queue].enter_sem = 1;      /* give back control */
    return i;
}

/* return a count of slots open for use in a given queue */
int slots_free(int w_queue)
{
    int i, count = 0;

    for (i = queues[w_queue].low_slot; i < queues[w_queue].high_slot; i++)
        if (players[i].p_status == PFREE)
            count++;
    return count;
}

/* Return a count of slots playing in a given queue, but do not report
   slots with no team unless showempty is set, or those in an overlapping
   queue. */
int slots_playing(int w_queue, int showempty)
{
    int i, count = 0;

    for (i = queues[w_queue].low_slot; i < queues[w_queue].high_slot; i++)
        if ((players[i].p_status != PFREE) &&
            !is_robot(&players[i]) &&
            (players[i].w_queue == w_queue) &&
            ((players[i].p_team != ALLTEAM) || showempty))
            count++;
    return count;
}

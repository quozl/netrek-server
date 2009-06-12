/* 
 * death.c
 */
#include "copyright.h"

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"
#include "sigpipe.h"
#include "ltd_stats.h"

extern int living;

void death(void)
{
    sigpipe_suspend(SIGALRM);

    me->p_status = POUTFIT;             /* Stop the ghost buster */
    me->p_flags &= ~PFTWARP;

    switch(me->p_whydead) {
    case KTORP:
    case KTORP2:
    case KPLASMA:
    case KPLASMA2:
    case KPHASER:
    case KPLANET:
    case KSHIP:
    case KSHIP2:
    case KGENOCIDE:
    case KGHOST:
    case KPROVIDENCE:
    case TOURNEND:
    case TOURNSTART:
	break;
    case KWINNER:
        if (genoquit && (queues[QU_PICKUP].q_flags & QU_OPEN) &&
	    (queues[QU_PICKUP].count >= genoquit) &&
	    !(me->p_flags & PFOBSERV)){
	  mustexit = 1;
	}
	break;
    case KQUIT:
    case KDAEMON:
    case KOVER:
    case KBADBIN:
        if (!(me->p_authorised && (me->p_flags & PFOBSERV)))
            mustexit = 1;
	break;
    default:
	ERROR(1,("BUG in server: ?? reason for death: %d\n",me->p_whydead));
	printf("BUG in server: ?? reason for death: %d\n",me->p_whydead);
	mustexit = 1;
	break;
    }

#ifdef LTD_STATS

    /* reset the LTD history stats */
    ltd_reset_hist(me);

    /* update the LTD totals group */
    ltd_update_totals(me);

#endif

    me->p_flags &= ~(PFWAR|PFREFITTING);

    /* all INL games advance at guest rate */    
    if (inl_mode) hourratio=5;

    /* First we check for promotions, unless in an INL draft game: */
    if ((me->p_stats.st_rank < NUMRANKS-1) &&
        !(status->gameup & GU_INL_DRAFTED)) {

#ifdef LTD_STATS

        if (ltd_can_rank(me))
          me->p_stats.st_rank++;

#else
        float ratingTotals;

        ratingTotals=planetRating(me) + bombingRating(me) + offenseRating(me);

        /* The person is promoted if they have enough hours and their ratings
         * are high enough, or if it is clear that they could sit around and
         * do nothing for the amount of time required, and still make the rank.
         * (i.e. their 'DI' rating is about rank.hours * rank.ratings ad they
         *  have insufficient time yet).
         */
        if ((offenseRating(me) >= ranks[mystats->st_rank + 1].offense || !offense_rank) &&
            ((mystats->st_tticks/36000.0 >= ranks[mystats->st_rank + 1].hours/hourratio &&
             ratingTotals >= ranks[mystats->st_rank + 1].ratings) ||
            (mystats->st_tticks/36000.0 < ranks[mystats->st_rank + 1].hours/hourratio &&
             ratingTotals*(mystats->st_tticks/36000.0) >=
             ranks[mystats->st_rank+1].hours/hourratio*ranks[mystats->st_rank+1].ratings))
) {
            mystats->st_rank ++;
        } else if 
	    /* We also promote if they belong in their current rank, but have
	     * twice enough DI for the next rank.
	     */
	    (((offenseRating(me) >= ranks[mystats->st_rank + 1].offense || !offense_rank) &&
	      ratingTotals >= ranks[mystats->st_rank].ratings) && 
	     ratingTotals*(mystats->st_tticks/36000.0) >= 
	     ranks[mystats->st_rank+1].hours/hourratio*ranks[mystats->st_rank+1].ratings*2) {
	    mystats->st_rank ++;
	} else if 
	    /* We also promote if they belong down a rank, but have four
	     * times the DI for the next rank.
	     */
	    (mystats->st_rank > 0 &&
	     ((offenseRating(me) >= ranks[mystats->st_rank + 1].offense || !offense_rank) &&
	      ratingTotals >= ranks[mystats->st_rank-1].ratings) &&
	     ratingTotals*(mystats->st_tticks/36000.0) >=
	     ranks[mystats->st_rank+1].hours/hourratio*ranks[mystats->st_rank+1].ratings*4) {
	    mystats->st_rank++;
        } else if ( (mystats->st_rank) >=4 ) {
            /* We also promote if they belong down a rank, but have eight
             * times the DI for the next rank, and at least the rank of 
             * Captain.
             */
	    if ((mystats->st_rank-3 > 0 &&
              ((offenseRating(me) >= ranks[mystats->st_rank + 1].offense || !offense_rank) &&
               ratingTotals >= ranks[mystats->st_rank-2].ratings) &&
	       ratingTotals*(mystats->st_tticks/36000.0) >=
	       ranks[mystats->st_rank+1].hours / 
	       hourratio*ranks[mystats->st_rank+1].ratings*8)) {
		mystats->st_rank++;
            }
        }

#endif /* LTD_STATS */

    }
    updateClient();
    /* Give them one more update now... */
    savestats();
    living = 0;
    return;
}

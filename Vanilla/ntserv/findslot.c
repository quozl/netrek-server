/* 
 * findslot.c
 *
 * Kevin Smith 03/23/88
 * Heavily modified my Michael Kantner 10/94
 *
 */
#include "copyright2.h"

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "proto.h"


/*
 * The following code for findslot() is really nice.
 *
 * Returns either the player slot number or -1 if it cannot give a slot.
 *
 * Every process will wait for a second (sleep(1)), and then check the queue
 *   status, do what is appropriate, and repeat.
 *
 * Variables:
 *   queue->count:  (shared mem) Next number to take.
 *
 *   oldcount:      (local) My position last time I looked.
 */

int findslot(int w_queue)
{
    u_int oldcount;	/* My old number */
    pid_t mypid = getpid();
    int i;		
    int rep = 0;
    int mywait = -1;
    
    /* Ensure that the queue is open */
    if (!(queues[w_queue].q_flags & QU_OPEN)) return (-1);

    /* If no one is waiting, I will try to enter now */
    if (queues[w_queue].first == -1)
	if ( (i=pickslot(w_queue)) >= 0 )  return (i);

    mywait = queue_add(w_queue);    /* Get me into the queue */
    if (mywait == -1) return (-1);   /* The queue is full! */

    oldcount = waiting[mywait].count;

    for (;;) {
        /* Check to ensure that the player still owns his wait slot */
        if (mypid != waiting[mywait].process){
          ERROR(1,("findslot: process lost its waitq entry!\n"));
          mywait = queue_add(w_queue);        /* Get me into the queue */
          if (mywait == -1) return (-1);    /* The queue is full! */
          /* The count should change, so a new packet will be sent. */
        }

	/* If we are first in line, check for a free slot */
	if (mywait == queues[w_queue].first){
	    if ((i=pickslot(w_queue)) >= 0 ) {
		queue_exit(mywait);
		return (i);
	    }
	} /* End of if I am first in line */

	/* Send packets occasionally to see if he is accepting... */
	if (rep++ % 10 == 0) {
	    sendQueuePacket((short) waiting[mywait].count);
	}
	
	if (isClientDead()) {
	    queue_exit(mywait);
	    fflush(stderr);
	    exit(0);
	}

	if (waiting[mywait].count != oldcount) {  /* If my count changed */
	    sendQueuePacket((short) waiting[mywait].count);
	    oldcount=waiting[mywait].count;
	}

	/* Message from daemon that it died (is going down) */
	if (!(status->gameup & GU_GAMEOK)) {
	    queue_exit(mywait);
	    return (-1);
	}

	/* To mimize process overhead and aid synchronization */
	sleep(1);

    }
/*    return -1; compiler warning: statement unreachable */
}

#include <stdio.h>
#include <sys/types.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

extern int openmem(int);

int main(void)
{

    int i, next;
    struct queuewait *lwait;

    openmem(0);
    printf("Status tourn: %u \n",status->tourn);

    printf("    Queue name (number)   sem  max  free  cnt  flags");
    printf("  first,last \n");
    for (i=0; i < MAXQUEUE; i++){
	printf("%20s(%1i)    %2i   %2i    %2i   %2u   %4i  %3i,%3i\n",
	       queues[i].q_name,
	       i,
	       queues[i].enter_sem,
	       queues[i].max_slots,
	       queues[i].free_slots,
	       queues[i].count,
	       queues[i].q_flags,
	       queues[i].first,
	       queues[i].last);
	next = queues[i].first;
	while (next != -1){
   	    lwait = &(waiting[next]);
	    printf("  Process %i, ",lwait->process);
	    printf("  prev %3i,",lwait->previous); 
	    printf("  me:  %3i,",lwait->mynum);
	    printf("  next %3i,\n",lwait->next);
	    next = lwait->next;
	}
    }

    return 1;		/* satisfy lint */
}

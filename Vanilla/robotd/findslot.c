/* 
 * findslot.c
 *
 * Kevin Smith 03/23/88
 *
 */
#include "copyright2.h"

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include "struct.h"
#include "data.h"

findslot()
{
    int oldcount= -1;

    /* Wait for some kind of indication about in/not in */
    while (queuePos==-1) {
	socketPause();
	if (isServerDead()) {
	    mprintf("Augh!  Ghostbusted!\n");
	    exit(0);
	}
	readFromServer(0);
	if (me!=NULL) {
	    /* We are in! */
	    return(me->p_no);
	}
    }

    for (;;) {
	socketPause();
	readFromServer(0);
	if (isServerDead()) {
	    mprintf("We've been ghostbusted!\n");
	    exit(0);
	}
	if (queuePos != oldcount) {
	    printf("%d ", queuePos); fflush(stdout);
	    oldcount=queuePos;
	}
	if (me!=NULL) {
	    return(me->p_no);
	}
    }
}

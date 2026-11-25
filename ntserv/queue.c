#include "config.h"
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "ip.h"
#include "proto.h"

/*
 * queue.c:  Wait queue handling routines
 *   The waitq is handled in a deterministic manner by a double linked
 *   list.  Extra redundancy is built into the code, to account for the
 *   unexpected.  All ERROR messages should never appear in the log file.
 *   If these occur, the linked list structure has been violated somehow.
 *
 *   written by Michael Kantner, October 1994
 *   updated:
 *   mjk - March 1995 - added extra redundancy, fixed a tiny bug.
 *
 */

/* Function headers, for when the server becomes ansi (or -Wall) */
int queues_init(void);
int queue_add(int w_queue);
int queue_exit(int waitindex);
int queue_update(int w_queue);
int queues_purge(void);
int queue_setname(int w_queue, char *name);


/*
 * Set up the the wait queues initially for pickup game
 */
int queues_init(void)
{
    register int i;

    /* Initialize all waitings */
    for (i=0;i<MAXWAITING;i++) {
        waiting[i].mynum = i;
	waiting[i].inuse = 0;
	waiting[i].previous = -1;
	waiting[i].next = -1;
	waiting[i].count = -1;  /* Does this make sense for a u_int? */
	waiting[i].w_queue = -1;
	waiting[i].process = 0;
    }


    /* Initialize all queues */
    for (i=0;i<MAXQUEUE;i++) {
	queues[i].enter_sem  = 1;
	queues[i].free_slots = 0;
	queues[i].max_slots  = 0;
	queues[i].tournmask  = NOBODY;
	queues[i].low_slot   = 0;
	queues[i].prefer     = 0;
	queues[i].high_slot  = 0;
	queues[i].first      = -1;
	queues[i].last       = -1;
    	queues[i].count      = 0;
	queues[i].q_flags    = 0;
	queue_setname(i,"no name");
      }

    /* Setup the pickup queue */
    queues[QU_PICKUP].free_slots = MAXPLAYER - TESTERS;
    queues[QU_PICKUP].max_slots  = MAXPLAYER - TESTERS;
    queues[QU_PICKUP].tournmask  = ALLTEAM;
    queues[QU_PICKUP].low_slot   = 0;
    queues[QU_PICKUP].high_slot  = MAXPLAYER - TESTERS;
    queues[QU_PICKUP].q_flags    = QU_OPEN|QU_RESTRICT|QU_REPORT;
    queue_setname(QU_PICKUP,"pickup");

    queues[QU_ROBOT].free_slots = TESTERS;
    queues[QU_ROBOT].max_slots  = TESTERS;
    queues[QU_ROBOT].tournmask  = ALLTEAM;
    queues[QU_ROBOT].low_slot   = MAXPLAYER - TESTERS;
    queues[QU_ROBOT].high_slot  = MAXPLAYER;
    queues[QU_ROBOT].q_flags    = QU_OPEN;
    queue_setname(QU_ROBOT,"robot");

    queues[QU_HOME].free_slots = 0; /* Off by default, inlbot gives 8 */
    queues[QU_HOME].max_slots  = (MAXPLAYER - TESTERS) / 2;
    queues[QU_HOME].tournmask  = FED;
    queues[QU_HOME].low_slot   = 0;
    queues[QU_HOME].prefer     = 0;
    queues[QU_HOME].high_slot  = MAXPLAYER - TESTERS;
    queues[QU_HOME].q_flags    = QU_REPORT;
    queue_setname(QU_HOME,"home");

    queues[QU_AWAY].free_slots = 0; /* Off by default, inlbot gives 8 */
    queues[QU_AWAY].max_slots  = (MAXPLAYER - TESTERS) / 2;
    queues[QU_AWAY].tournmask  = ROM;
    queues[QU_AWAY].low_slot   = 0;
    queues[QU_AWAY].prefer     = 8;
    queues[QU_AWAY].high_slot  = MAXPLAYER - TESTERS;
    queues[QU_AWAY].q_flags    = QU_REPORT;
    queue_setname(QU_AWAY,"away");

    queues[QU_HOME_OBS].free_slots = 0;  /* Off by default, inlbot gives 2 */
    queues[QU_HOME_OBS].max_slots  = TESTERS / 2;
    queues[QU_HOME_OBS].tournmask  = FED;
    queues[QU_HOME_OBS].low_slot   = MAXPLAYER - TESTERS;
    queues[QU_HOME_OBS].high_slot  = MAXPLAYER - (TESTERS / 2);
    queues[QU_HOME_OBS].q_flags    = QU_OBSERVER|QU_REPORT;
    queue_setname(QU_HOME_OBS,"home observer");

    queues[QU_AWAY_OBS].free_slots = 0;  /* Off by default, inlbot gives 2 */
    queues[QU_AWAY_OBS].max_slots  = TESTERS / 2;
    queues[QU_AWAY_OBS].tournmask  = ROM;
    queues[QU_AWAY_OBS].low_slot   = MAXPLAYER - (TESTERS / 2);
    queues[QU_AWAY_OBS].high_slot  = MAXPLAYER;
    queues[QU_AWAY_OBS].q_flags    = QU_OBSERVER|QU_REPORT;
    queue_setname(QU_AWAY_OBS,"away observer");

    queues[QU_PICKUP_OBS].free_slots = TESTERS - 1; /* Give 1 less than TESTERS, so a robot can always */
    queues[QU_PICKUP_OBS].max_slots  = TESTERS - 1; /* join the game */
    queues[QU_PICKUP_OBS].tournmask  = ALLTEAM;
/* if ROBOT_OBSERVER_OFFSET defined we force observers to start in slot 'h' */
/* thereby forcing Smack and other moderation bots to start in slot 'g' */
#ifdef ROBOT_OBSERVER_OFFSET
    queues[QU_PICKUP_OBS].low_slot   = MAXPLAYER - TESTERS + 1;
#else
    queues[QU_PICKUP_OBS].low_slot   = MAXPLAYER - TESTERS;
#endif
    queues[QU_PICKUP_OBS].high_slot  = MAXPLAYER - 1;
    queues[QU_PICKUP_OBS].q_flags    =
      QU_OPEN|QU_RESTRICT|QU_OBSERVER|QU_REPORT;
    queue_setname(QU_PICKUP_OBS,"pickup observer");

    queues[QU_GOD].free_slots = MAXPLAYER;
    queues[QU_GOD].max_slots  = MAXPLAYER;
    queues[QU_GOD].tournmask  = ALLTEAM;
    queues[QU_GOD].low_slot   = 0;
    queues[QU_GOD].high_slot  = MAXPLAYER;
    queues[QU_GOD].q_flags    = QU_OPEN;
    queue_setname(QU_GOD,"god");

    queues[QU_GOD_OBS].free_slots = 1;
    queues[QU_GOD_OBS].max_slots  = 1;
    queues[QU_GOD_OBS].tournmask  = ALLTEAM;
    queues[QU_GOD_OBS].low_slot   = 29;
    queues[QU_GOD_OBS].high_slot  = 29;
    queues[QU_GOD_OBS].q_flags    = QU_OPEN|QU_OBSERVER|QU_TOK;
    queue_setname(QU_GOD_OBS,"god observer");

#ifdef NEWBIESERVER
    queues[QU_NEWBIE_PLR].free_slots = MAXPLAYER - TESTERS;
    queues[QU_NEWBIE_PLR].max_slots = MAXPLAYER - TESTERS;
    queues[QU_NEWBIE_PLR].tournmask = ALLTEAM;
    queues[QU_NEWBIE_PLR].low_slot = 0;
    queues[QU_NEWBIE_PLR].high_slot = MAXPLAYER - TESTERS;
    queues[QU_NEWBIE_PLR].q_flags = QU_OPEN|QU_RESTRICT;
    queue_setname(QU_NEWBIE_PLR, "newbie player");

    queues[QU_NEWBIE_BOT].free_slots = MAXPLAYER - TESTERS;
    queues[QU_NEWBIE_BOT].max_slots = MAXPLAYER - TESTERS;
    queues[QU_NEWBIE_BOT].tournmask = ALLTEAM;
    queues[QU_NEWBIE_BOT].low_slot = (MAXPLAYER - TESTERS) / 2;
    queues[QU_NEWBIE_BOT].high_slot = MAXPLAYER - (TESTERS / 2);
    queues[QU_NEWBIE_BOT].q_flags = QU_OPEN;
    queue_setname(QU_NEWBIE_BOT, "newbie robot");

    queues[QU_NEWBIE_OBS].free_slots = (MAXPLAYER - TESTERS) / 2 - 1;
    queues[QU_NEWBIE_OBS].max_slots = (MAXPLAYER - TESTERS) / 2 - 1;
    queues[QU_NEWBIE_OBS].tournmask = ALLTEAM;
    queues[QU_NEWBIE_OBS].low_slot = MAXPLAYER - (TESTERS / 2);
    queues[QU_NEWBIE_OBS].high_slot = MAXPLAYER - 1;
    queues[QU_NEWBIE_OBS].q_flags = QU_OPEN|QU_RESTRICT|QU_OBSERVER;
    queue_setname(QU_NEWBIE_OBS, "newbie observer");

    queues[QU_NEWBIE_DMN].free_slots = 1;
    queues[QU_NEWBIE_DMN].max_slots = 1;
    queues[QU_NEWBIE_DMN].tournmask = ALLTEAM;
    queues[QU_NEWBIE_DMN].low_slot = MAXPLAYER - 1;
    queues[QU_NEWBIE_DMN].high_slot = MAXPLAYER;
    queues[QU_NEWBIE_DMN].q_flags = QU_OPEN;
    queue_setname(QU_NEWBIE_DMN, "newbie daemon");
#endif
#ifdef PRETSERVER
    queues[QU_PRET_DMN].free_slots = 1;
    queues[QU_PRET_DMN].max_slots = 1;
    queues[QU_PRET_DMN].tournmask = ALLTEAM;
    queues[QU_PRET_DMN].low_slot = MAXPLAYER - 1;
    queues[QU_PRET_DMN].high_slot = MAXPLAYER;
    queues[QU_PRET_DMN].q_flags = QU_OPEN;
    queue_setname(QU_PRET_DMN, "preT daemon");
#endif

    return 1;
}


static int queue_update_locked(int w_queue);

/*
 *  Enter an ntserv in to the end of line
 */
int queue_add(int w_queue)
{
    struct pqueue *lqueue = &queues[w_queue];
    struct queuewait *lwait;
    int i;
    int hidden;

    /* Ensure that the queue is open */
    if (!(queues[w_queue].q_flags & QU_OPEN)) return -1;

    hidden = ip_hide(ip);

    /* Hold LOCK_QUEUE_ADD across allocation and linked-list insertion */
    lock_on(LOCK_QUEUE_ADD);
    for (i=0;i<MAXWAITING;i++){
        if (waiting[i].inuse == 0) break;
    }

    if (i == MAXWAITING) {
        lock_off(LOCK_QUEUE_ADD);
        return -1;
    }

    lwait = &(waiting[i]);
    lwait->inuse = 1;

    /* If I am the first, do special thing */
    if (lqueue->first == -1) {
	lqueue->first = i;
	lwait->previous = -1;
    } else {
	waiting[lqueue->last].next = i;
	lwait->previous = lqueue->last;
    }

    lqueue->last       = i;
    lwait->next        = -1;
    lwait->count       = lqueue->count++;
    lwait->process     = getpid();
    lwait->w_queue     = w_queue;
    if (hidden) {
        strcpy(lwait->ip, "127.0.0.1");
        strcpy(lwait->host, "hidden");
    } else {
        strncpy(lwait->ip, ip, sizeof(lwait->ip) - 1);
        lwait->ip[sizeof(lwait->ip) - 1] = '\0';
        strncpy(lwait->host, host, sizeof(lwait->host) - 1);
        lwait->host[sizeof(lwait->host) - 1] = '\0';
    }
    lock_off(LOCK_QUEUE_ADD);

    if (!hidden) {
        ip_lookup(ip, lwait->host, lwait->host,
                  NULL, NULL, NULL, sizeof(lwait->host));
    }

    return i;
}


/*
 *  Remove an ntserv from waiting (either into game or quit)
 */
int queue_exit(int waitindex)
{
    struct pqueue *lqueue;
    struct queuewait *lwait;
    int w_queue;

    if (waitindex < 0 || waitindex >= MAXWAITING) {
      ERROR(1,("queue_exit: Invalid waitindex %i\n", waitindex));
      return 0;
    }

    lock_on(LOCK_QUEUE_ADD);
    lwait = &(waiting[waitindex]);
    if (!lwait->inuse) {
      lock_off(LOCK_QUEUE_ADD);
      return 0;
    }
    w_queue = lwait->w_queue;

    if ((w_queue < 0) || (w_queue >= MAXQUEUE)){
      lock_off(LOCK_QUEUE_ADD);
      ERROR(1,("queue_exit: Invalid queue number %i\n",w_queue));
      return 0;
    }

    lqueue = &queues[w_queue];

    if (lwait->previous == -1){ 
	lqueue->first = lwait->next;
    }
    else if (lwait->previous >= 0 && lwait->previous < MAXWAITING){
	waiting[lwait->previous].next = lwait->next;
    }

    if (lwait->next == -1){
	lqueue->last = lwait->previous;
    }
    else if (lwait->next >= 0 && lwait->next < MAXWAITING){
	waiting[lwait->next].previous = lwait->previous;
    }

    /* Clear the lwait for redundancy */
    lwait->previous = -1;
    lwait->next = -1;
    lwait->count = -1;
    lwait->w_queue = -1;
    lwait->process = 0;
    lwait->inuse = 0;

    queue_update_locked(w_queue);  /* Update everyone's count */
    lock_off(LOCK_QUEUE_ADD);
    ip_waitpid();

    return 1;
}

/*
 *  Set the name of the queue in a safe manner.
 */

int queue_setname(int w_queue, char *name)
{
    if ((w_queue < 0) || (w_queue >= MAXQUEUE)){
      ERROR(1,("queue_update: Invalid queue number %i\n",w_queue));
      return 0;
    }
    
    strncpy(queues[w_queue].q_name,name,QNAMESIZE-1);
    queues[w_queue].q_name[QNAMESIZE-1] = '\0';
    return 1;
}

/*
 *  Update all of the waiting counts for a given queue (caller holds LOCK_QUEUE_ADD)
 */
static int queue_update_locked(int w_queue)
{
    struct pqueue *lqueue;
    int currentindex = -1;
    struct queuewait *current;
    u_int lcount = 0;

    if ((w_queue < 0) || (w_queue >= MAXQUEUE)){
      ERROR(1,("queue_update: Invalid queue number %i\n",w_queue));
      return 0;
    }

    lqueue = &queues[w_queue];
    currentindex = lqueue->first;
    while (currentindex >= 0 && currentindex < MAXWAITING){
        current = &(waiting[currentindex]);
        if (lcount >= MAXWAITING){   /* This is redundant */
	    ERROR(1,("queues_update: wait count more than MAXWAITING\n"));
	    break;  /* Exit from the while loop */
	}
	if (current->inuse == 0){   /* This is redundant */
  	    ERROR(1,("queues_update: waitq element is not in use\n"));
	    break;  /* Exit from the while loop */
	}
	if (current->process == 0){   /* This is redundant */
	    ERROR(1,("queues_update: waitq element is process 0\n"));
	    break;  /* Exit from the while loop */
	}
	/* The waitq element appears valid */
	current->count=lcount++;
	currentindex = current->next;
    }
    lqueue->count=lcount;

    return 1;
}

/*
 *  Update all of the waiting counts for a given queue
 */
int queue_update(int w_queue)
{
    int ret;

    if ((w_queue < 0) || (w_queue >= MAXQUEUE)){
      ERROR(1,("queue_update: Invalid queue number %i\n",w_queue));
      return 0;
    }
    lock_on(LOCK_QUEUE_ADD);
    ret = queue_update_locked(w_queue);
    lock_off(LOCK_QUEUE_ADD);
    return ret;
}

/*
 *  Remove dead processes from the queue
 */
int queues_purge(void)
{
    register int i;
    int killcount = 0, next = -1;
    struct pqueue *lqueue;
    struct queuewait *lwait;
    int dead[MAXWAITING];
    int ndead = 0, d;

    lock_on(LOCK_QUEUE_ADD);
    for (i=0, lqueue=&(queues[0]); i<MAXQUEUE;i++,lqueue++){
        next = lqueue->first;
	while (next >= 0 && next < MAXWAITING){
            lwait = &(waiting[next]);
	    if (killcount++ >= MAXWAITING){   /* This is redundant */
	      ERROR(1,("queues_purge: trying to kill more than MAXWAITING\n"));
	      lock_off(LOCK_QUEUE_ADD);
	      return 0;  /* Exit from queues_purge */
	    }
	    if (lwait->inuse == 0){   /* This is redundant */
	      ERROR(1,("queues_purge: waitq element is not in use\n"));
	      break;  /* Stop scanning this queue */
	    }
	    if (lwait->process == 0){   /* This is redundant */
	      ERROR(1,("queues_purge: waitq element is process 0\n"));
	      break;  /* Stop scanning this queue */
	    }
	    /* The waitq element appears valid */
	    next = lwait->next;
	    if (kill(lwait->process, 0) < 0){
	        if (errno == ESRCH){
                    /* The process is not a valid number */
		    ERROR(1,("queues_purge: removing dead waitQ player\n"));
		    if (ndead < MAXWAITING)
			dead[ndead++] = lwait->mynum;
		}
		else if (errno == EINVAL){
		    /* The value of 0 is not acceptable!!! */
                    ERROR(1,("queues_purge: EINVAL kill error!\n"));
	        }
		else if (errno == EPERM){
		    /* Permission error */
		    ERROR(1,("queues_purge: EPERM kill error (pid %i)!\n",
			     lwait->process));
		}
                else{
		    /* Unexpected error value */
		    ERROR(1,("queues_purge: Unknown kill error %i\n",errno));
		}
	    }
	}
    }
    lock_off(LOCK_QUEUE_ADD);

    for (d = 0; d < ndead; d++) {
	queue_exit(dead[d]);
    }

    lock_on(LOCK_PICKSLOT);
    for (i=0, lqueue=&(queues[0]); i<MAXQUEUE; i++, lqueue++){
	if (lqueue->free_slots > lqueue->max_slots){
	  /* The queue has more free slots than allowed. */
	  /* This is typically caused by ghostbusting and such */
	  /* Ideally, this never happens, but occasionally it does */
	  ERROR(1,("queues_purge: reducing the free_slots to max_slots\n"));
	  lqueue->free_slots = lqueue->max_slots;
	}
    }
    lock_off(LOCK_PICKSLOT);

    return 1;
}

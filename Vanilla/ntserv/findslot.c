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
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "ip.h"
#include "packets.h"
#include "proto.h"
#include "blog.h"
#include "util.h"
#include "slotmaint.h"

/* return number of playing hosts with same ip */
static int playing_count_by_ip(int w_queue) {
  int i, j;
  for (i=0, j=0; i<MAXPLAYER; i++) {
    if (players[i].p_status == PFREE) continue;
    if (is_robot(&players[i])) continue;
    /* if we want a pickup slot, ignore any observer slot we have */
    if (w_queue == QU_PICKUP && players[i].p_status == POBSERV) continue;
    if (players[i].p_ip_duplicates) continue;
    if (strcmp(players[i].p_ip, ip) == 0) j++;
  }
  return j;
}

/* return number of waiting hosts with same ip */
static int waiting_count_by_ip(int w_queue) {
  int i, j, k, n;
  n = queues[w_queue].count;
  if (n == 0) return n;
  k = queues[w_queue].first;
  for (i=0, j=0; i<n; i++) {
    if (strcmp(waiting[k].ip, ip) == 0) j++;
    k = waiting[k].next;
  }
  return j;
}

/* kill all player processes with same ip */
static void free_duplicate_ips() {
  int i, pid;
  for (i=0; i<MAXPLAYER; i++) {
    if (strcmp(players[i].p_ip, ip) == 0) {
      pid = players[i].p_process;
      if (pid != 0) {
        ERROR(2,("free_duplicate_ips: process %d killed\n", pid));
        kill(players[i].p_process, SIGTERM);
      }
    }
  }
}

/*
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
    if (!(queues[w_queue].q_flags & QU_OPEN)) return -1;

    /* unfair pre-queue delay if client ip is banned */
    if ((w_queue != QU_PICKUP_OBS) && (bans_check_temporary(ip))) {
      int elapsed = ban_vote_duration - bans_check_temporary_remaining();
      if (elapsed > 60) {
	for (;;) {
	  int remaining = bans_check_temporary_remaining();
	  if (remaining == 0) break;
	  if (rep++ % 10 == 1) { sendQueuePacket((short) remaining); }
	  if (isClientDead()) { fflush(stderr); exit(0); }
	  if (!(status->gameup & GU_GAMEOK)) { return -1; }
	  sleep(2);
	}
      }
    }

    /* unfair pre-queue if client from same ip address is already queued */
    if ((w_queue == QU_PICKUP) || (w_queue == QU_PICKUP_OBS)
     || (w_queue == QU_NEWBIE_PLR) || (w_queue == QU_NEWBIE_OBS)) {
      for (;;) {
	int n = waiting_count_by_ip(w_queue);
	if (n < 1) break;
	if (rep++ % 10 == 1) { sendQueuePacket((short) 2000+n); }
	if (isClientDead()) { fflush(stderr); exit(0); }
	if (!(status->gameup & GU_GAMEOK)) { return -1; }
	sleep(1);
      }
    }

    /* unfair pre-queue if client from same ip address is already playing */
    if ((w_queue == QU_PICKUP) || (w_queue == QU_PICKUP_OBS)
     || (w_queue == QU_NEWBIE_PLR) || (w_queue == QU_NEWBIE_OBS)) {
      for (;;) {
	int n = playing_count_by_ip(w_queue);
	if (n <= duplicates) break;
	if (deny_duplicates) {
	  ip_deny_set(ip);
	  ERROR(2,("findslot: %s added to denied parties list\n", ip));
	  free_duplicate_ips();
	  return -1;
	} else {
	  if (rep++ % 10 == 1) { sendQueuePacket((short) 1000+n); }
	}
	if (isClientDead()) { fflush(stderr); exit(0); }
	if (!(status->gameup & GU_GAMEOK)) { return -1; }
	sleep(1);
      }
    }

    /* If no one is waiting, I will try to enter now */
    if (queues[w_queue].first == -1) {
      if ((i=pickslot(w_queue)) >= 0) {
        if (w_queue == QU_PICKUP) {
          if (queues[w_queue].free_slots == 0) {
            blog_pickup_game_full();
          } else {
            if (queues[w_queue].free_slots > 8) {
              blog_pickup_game_not_full();
            }
          }
        }
        return i;
      }
    }


    mywait = queue_add(w_queue);    /* Get me into the queue */
    if (mywait == -1) {
      if (w_queue == QU_PICKUP) blog_pickup_queue_full();
      return -1; /* queue full */
    }
    if (w_queue == QU_PICKUP) blog_pickup_queue_not_full();

    rep = 0;
    oldcount = waiting[mywait].count;

    for (;;) {
        /* Check to ensure that the player still owns his wait slot */
        if (mypid != waiting[mywait].process){
          ERROR(1,("findslot: process lost its waitq entry!\n"));
          mywait = queue_add(w_queue);        /* Get me into the queue */
          if (mywait == -1) return -1;        /* The queue is full! */
          /* The count should change, so a new packet will be sent. */
        }

	/* If we are first in line, check for a free slot */
	if (mywait == queues[w_queue].first){
	    if ((i=pickslot(w_queue)) >= 0 ) {
		queue_exit(mywait);
		return i;
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
	    return -1;
	}

	/* To mimize process overhead and aid synchronization */
	sleep(1);

    }
}

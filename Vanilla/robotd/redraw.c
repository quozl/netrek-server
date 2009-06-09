/*
 * redraw.c
 */
#include "copyright.h"

#include <stdio.h>
#include <signal.h>
#include <math.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

int	_tcheck;

intrupt()
{
   static long     lastread;
   static int      prevread;
   int		   cr_time;

   /* handle torp firing */
   do_torps();
   _tcheck = 0;
keep_reading:
	;
#ifdef ATM
   if (readFromServer(pollmode)) {	/* should be 0 */
#else
   if (readFromServer()) {
#endif
      cr_time = mtime(0);

      _cycletime = cr_time - prevread;
      _serverdelay = ((double)_cycletime/100.);
      _udnonsync++;

      /* can't be less than 100 ms */
      if(_serverdelay < 1.) _serverdelay = 1.;

      if (DEBUG & DEBUG_SERVER) {
	 printf("cycletime = %dms\n", _cycletime);
	 printf("wait for input = %dms\n", cr_time - _waiting_for_input);
      }
      prevread = cr_time;

      if(_state.borg_detect)
	 borg_detect();
   } else {
      /*
       * We haven't heard from server for 3 secs... Strategy:  send a
       * useless packet to "ping" server.
       */
      /* If server is dead, just give up */
      if (isServerDead())
         exitRobot(0);
      if(!pollmode){
	 int	now = time(NULL)-3;
	 mprintf("sending wakeup packet at %s", ctime(&now));
	 sendWarReq(me->p_hostile);
      }
   }
   if (me->p_status == POUTFIT) {
      death();
   }
   if(me->p_status != PALIVE)
      goto keep_reading;
}

/* borg detect */

borg_detect()
{
   register Player		*p;
   register struct player	*j;
   int				last_speedc, last_dirc;
   register			i;

   for(i=0, p=_state.players; i < MAXPLAYER; i++,p++){
      if(!p->p || (p->p->p_status != PALIVE) || p->invisible)
	 continue;
      
      j = p->p;

      /* check for simultaneous torp & phaser but in different directions.
	 -- Very common borg maneuver */
      if(_udnonsync - p->tfire_t < 2){
	 if(p->tfire_t == p->pfire_t){
	    if(p->tfire_dir > -1){
	       if(angdist(p->tfire_dir, p->pfire_dir) > 2){
		  /*
		  printf("%s BORG %d\n", j->p_mapchars, 
		     angdist(p->tfire_dir, p->pfire_dir));
		  printf("current p: %d\n", p->bp1);
		  */
		  p->bp1 ++;
	       }
	    }
	 }
      }
      /* check for recent change in direction far away from current torp
	 or phaser fire */
      if(j->p_speed > 1){
	 if(_udnonsync - p->tfire_t < 2){
	    if(_udnonsync - p->turn_t < 2){
	       if(angdist(j->p_dir, p->tfire_t) > 64){
		  /*
		  printf("%s tfire BORG %d\n", j->p_mapchars,
		     angdist(j->p_dir, p->tfire_t));
		  */
		  p->bp2 ++;
	       }
	    }
	 }
	 if(_udnonsync - p->pfire_t < 2){
	    if(_udnonsync - p->turn_t < 2){
	       if(angdist(j->p_dir, p->pfire_t) > 64){
		  /*
		  printf("%s pfire BORG %d\n", j->p_mapchars,
		     angdist(j->p_dir, p->pfire_t));
		  */
		  p->bp3 ++;
	       }
	    }
	 }
      }
      if(p->bdc < 1.0)
	 p->borg_probability = 0.0;
      else
	 p->borg_probability = (double)(p->bp1 * 10 + p->bp2 + p->bp3)/
	    (double)p->bdc;
   }
}

/* maybe later */
#ifdef nodef
      if(_udcounter - p->tfire_t < 3){
	 
	 last_speedc = p->tfire_t - p->speed_t;
	 /* fired, then observed speed change within 5 cycles */
	 if(last_speedc < 5 && p->tfire_dir > -1){

	    /* player direction and torp fire direction greater than 64 */
	    if(angdist(j->p_dir, p->tfire_dir) > 64){
	       mprintf("speed detect\n");
	       p->borg_probability ++;
	    }
	 }
	 last_dirc = p->tfire_t - p->turn_t;
	 /* fired, then observed direction change within 5 cycles */
	 if(last_dirc < 5 && p->tfire_dir > -1){

	    /* player direction and torp fire direction greater than 64 */
	    if(angdist(j->p_dir, p->tfire_dir) > 64){
	       mprintf("direction detect\n");
	       p->borg_probability ++;
	    }
	 }
      }
#endif

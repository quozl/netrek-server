#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include "defs.h"
#include "struct.h"
#include "data.h"
#include "packets.h"
#include "robot.h"

#define ESCORT1_DIST		20000

/* escort dist to planet */
static int	_esc_dist;

/* escort player to planet */
escort()
{
   Player		*escp  = _state.escort_player;
   struct planet 	*escpl = _state.escort_planet;

   if(!_state.escort_req){
      mprintf("bad state.\n");
      return;
   }

   if(!escp || !escp->p || !isAlive(escp->p) || !escpl){
      if(DEBUG & DEBUG_ESCORT)
	 printf("escort: no escort.\n");
      rescort_c("no escortee/planet.");
      return;
   }

   if((escp->p->p_flags & PFORBIT) && !hostilepl(&planets[escp->p->p_planet])){
      rescort_c("orbiting .. either taking or refueling\n");
      return;
   }

   if(!unknownpl(escpl) && escpl->pl_owner == me->p_team){
      rescort_c("planet taken.");
      return;
   }

   /* set up variables used */
   _esc_dist = dist_to_planet(escp, escpl);

   /* check internal state */
   switch(_state.escort){
      case 0:
	 req_cloak_off("escort: 0");
	 /* just got request, need to go to planet */
	 if(DEBUG & DEBUG_ESCORT)
	    printf("goto escort planet %s\n", escpl->pl_name);
	 goto_escort_planet(escp, escpl);
	 break;

      case 1:
	 /* close enough to escort planet .. wait for escortee */
	 req_cloak_off("escort: 1");
	 if(DEBUG & DEBUG_ESCORT)
	    printf("wait for escort %s\n", escp->p->p_mapchars);
	 wait_for_escort(escp, escpl);
	 break;

      case 2:
	 /* ogg defenders */
	 if(DEBUG & DEBUG_ESCORT)
	    printf("begin planet %s goto defender\n", escpl->pl_name);
	 goto_defender(escp, escpl);
	 break;
      
      case 3:
	 /* uncloak and distract */
	 if(DEBUG & DEBUG_ESCORT)
	    printf("ogg defender\n");
	 ogg_defender(escp, escpl);
	 break;
      
      case 4:
	 break;
      
      case 5:
	 req_cloak_off("escort: 5");
	 /* do defenders */
	 if(DEBUG & DEBUG_ESCORT)
	    printf("no defenders\n");
	 rescort_c("no defenders");
	 break;
      
      default:
	 mprintf("bad internal escort state.\n");
	 break;
   }
}

/* states */

/*
 * Need to get to within a certain range of enemy planet while preserving
 * fuel.  Also need to consider how far escort player is to make sure we
 * get there first.
 */

/* 0 */
goto_escort_planet(p, pl)
   
   Player		*p;
   struct planet	*pl;
{
   unsigned char	crs = get_wrapcourse(pl->pl_x, pl->pl_y);
   Player		*e = _state.closest_e;
   int			speed, r=0;

   if(!e || !e->p || !isAlive(e->p)){
      e = NULL;
   }
   if(e) {
      if(e->dist < 15000 && e->dist > e->hispr)
	 r = avoiddir(e->crs, &crs, 32);
      else if(e->dist < e->hispr)
	 r = avoiddir(e->crs, &crs, 70);
   }
   
   if(_esc_dist > pl->pl_mydist)
      speed = myemg_speed()-2;
   else
      speed = myemg_speed();

   speed -= (angdist(me->p_dir, crs)/8);
   if(speed < 2) speed = 2;

   if(pl->pl_mydist < ESCORT1_DIST){
      _state.escort = 1;
   }

   do_course_speed(crs, speed);
}

/* 1 */
wait_for_escort(p, pl)
   
   Player		*p;
   struct planet	*pl;
{
   unsigned char	crs = get_wrapcourse(pl->pl_x, pl->pl_y);
   Player		*e = _state.closest_e;
   int			edist, r, speed;

   if(!e || !e->p || !isAlive(e->p)){
      e = NULL;
   }

   speed = myfight_speed();
   
   if(e) {
      if(e->dist < 15000 && e->dist > e->hispr)
	 r = avoiddir(e->crs, &crs, 32);
      else if(e->dist < e->hispr)
	 r = avoiddir(e->crs, &crs, 70);
      r = avoiddir(e->crs, &crs, 32);
      if(angdist(me->p_dir, e->crs) < 32)
	 speed = myfight_speed()-1;
      else if(angdist(me->p_dir, e->crs) > 100)
	 speed = myfight_speed()+1;
      else
	 speed = myfight_speed();

      if(e->dist > 15000)
	 speed = 1;
   }
   else
      /* just short of circle -- spiral */
      avoiddir(crs, &crs, 55);
   
   if(pl->pl_mydist > ESCORT1_DIST+6000){
      _state.escort = 0;
   }

   if(_esc_dist < pl->pl_mydist + /* fix */8000){
      _state.escort = 2;
   }

   do_course_speed(crs, speed);
}

/* 2 */
goto_defender(p, pl)
   
   Player		*p;
   struct planet	*pl;
{
   unsigned char 	crs;
   Player		*e = get_nearest_to_pl(pl);
   int			speed, r=0, edist;
   struct planet	*thp;

   if(!e || !e->p || !isAlive(e->p)){
      speed = myfight_speed();
      crs = to_team_planet(_state.warteam);
      _state.escort = 5;	/* none */
   }
   else{
      speed = myemg_speed();
      if(e->dist < _state.ogg_dist){
	 _state.escort = 3;
      } else if(e->dist > _state.ogg_dist * 2){
	 req_cloak_on();
      }
      crs = e->icrs;
   }
   do_course_speed(crs, speed);
}

/* 3 */
ogg_defender(p, pl)

   Player		*p;
   struct planet	*pl;
{
   unsigned char	crs;
   Player		*e = get_nearest_to_pl(pl);
   int			speed, r = 0, edist;

   req_cloak_off("begining escort ogg");
   if(!e || !e->p || !isAlive(e->p)){
      speed = myfight_speed();
      crs = to_team_planet(_state.warteam);
      _state.escort = 5;
   }
   else{
      if(running_away(e, e->crs, 32))
	 speed = e->p->p_speed + 1;
      else if(attacking(e, e->crs, 32))
	 speed = myfight_speed();
      else
	 speed = e->p->p_speed;
      crs = e->icrs;
   }
   do_course_speed(crs, speed);
}

/* UNUSED */
/* the further away the player is the more likely to be an interference. */

find_interference(ie, d)

   Player	*ie[MAXPLAYER];	/* used */
   int		d;
{
   register		i, j=0;
   register Player	*p;

   for(i=0, p=_state.players; i< MAXPLAYER; i++,p++){
      if(!p || !p->p || !isAlive(p->p) || !p->enemy) continue;
      if(p->dist >= d) continue;

      if(on_my_course(p))
	 ie[j++] = p;
   }
   return j;
}

on_my_course(p)

   Player	*p;
{
   unsigned char	mycrs = _state.p_desdir;
   unsigned char	ecrs  = p->crs;

   return angdist(mycrs, ecrs) < 16;
}

do_course_speed(crs, speed)
   unsigned char	crs;
   int			speed;
{
   unsigned char crs_r;
   int		 speed_r, hittime, lvorbit;

   compute_course(crs, &crs_r, speed, &speed_r, &hittime, &lvorbit);
   req_set_course(crs_r);
   req_set_speed(speed_r, __LINE__, __FILE__);
}

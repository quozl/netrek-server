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

torp_attack_robot(e, j, edist)

   Player		*e;
   struct player	*j;
   int			edist;
{
   int			dist, urnd;
   int			size=0;

   /* torp attack */

   attack_robot(e, j, edist);
}

phaser_attack_robot(e, j, edist)

   Player		*e;
   struct player	*j;
   int			edist;
{
   int	dist;
   unsigned char	pcrs;

   /* phaser attack */
   
   dist = PHASEDIST*me->p_ship.s_phaserdamage / 100 - 750;

#ifdef nodef
   if(dist > 5000)
      dist = 5000;
#endif
   
   if(edist < dist){
      
      if(phaser_condition(e, edist)){
	 int	x,y;
	 x = j->p_x + 
	    _avsdelay * (double)j->p_speed * Cos[j->p_dir] * WARP1;
	 y = j->p_y + 
	    _avsdelay * (double)j->p_speed * Sin[j->p_dir] * WARP1;
	 pcrs = get_course(x,y);
	 req_phaser(pcrs,0);
	 _state.battle[j->p_no] = mtime(0);
	 return;
      }
   }
}

unsigned char predict_dir(odir, ndir, dist)

   unsigned char	odir, ndir;
   int			dist;
{
   int	a = angdist(ndir, odir);
   int	d = odir + a;
   int	x;

   /* won't work for large distances */

   /* at 0 dist 0 angle */
   /* at 5000 dist 32 angle */
   /* at 6000 dist 64 angle */

   if(dist < 4500)
      x = (dist * 32) / 4500 + 12 - ship_factor(me->p_ship.s_type);
   else 
      x = ((dist - 3500) * 32) / 1000 + 10 - ship_factor(me->p_ship.s_type);
      /* too large? */
   
   d = NORMALIZE(d);
   if(d == ndir)
      return ndir + NORMALIZE(x);	/* should be already.. */
   else
      return ndir - NORMALIZE(x);
}

attack_robot(e, j, edist)
   
   Player		*e;
   struct player	*j;
   int			edist;
{
   int	iv;
   static unsigned char	olddir;
   unsigned char	tmpdir, tcrs;
   static int		dt;
   int			tm;

   _state.torp_i = 100;
   if(PL_DAMAGE(e) > 60 && edist < 10000){
      _timers.robot_attack = 0;
      _timers.pause_ra = 0;
      if(PL_DAMAGE(e) > 90 && edist < 10000){
	 if(DEBUG & DEBUG_WARFARE){
	    printf("damage 90 fire\n");
	 }
	 get_torp_course(j, &tcrs, me->p_x, me->p_y);
	 req_torp(tcrs);
      }
      else if(PL_DAMAGE(e) > 80 && edist < 8000){	/* XX */
	 if(DEBUG & DEBUG_WARFARE){
	    printf("damage 80 fire\n");
	 }
	 get_torp_course(j, &tcrs, me->p_x, me->p_y);
	 req_torp(tcrs);
      }
      else if(PL_DAMAGE(e) > 60 && edist < 8000){
	 if(DEBUG & DEBUG_WARFARE){
	    printf("damage 60 fire\n");
	 }
	 get_torp_course(j, &tcrs, me->p_x, me->p_y);
	 req_torp(tcrs);
      }
      return;
   }
   if(edist > 5250){
      _timers.robot_attack = 0;
      return;
   }

   if(edist <= 4750) 
      iv = 2;
   else
      iv = 1;
   
   /* too close */
   if(edist < 1500){
      if(DEBUG & DEBUG_WARFARE){
	 printf("close fire\n");
      }
      get_torp_course(j, &tcrs, me->p_x, me->p_y);
      _timers.robot_attack = 0;
      _timers.pause_ra = 0;
      req_torp(tcrs);
      return;
   }

#define PHASE1		(_timers.robot_attack < 2)

   if(PHASE1){
     if(/*(j->p_speed < 5) &&*/ !(j->p_flags & PFCLOAK) && 
	attacking(e, me->p_dir, 128)){
	 get_torp_course(j, &tcrs, me->p_x, me->p_y);
	 olddir = j->p_dir;
	 if(DEBUG & DEBUG_WARFARE){
	    printf("phase I %d\n",_timers.robot_attack);
	 }
	 _timers.robot_attack++;
	 req_torp(tcrs);
	 return;
      }
      else{
	 if(DEBUG & DEBUG_WARFARE){
	    printf("not attacking\n");
	 }
	 return;
      }
   }

#define PHASE2		(_timers.robot_attack >= 2 && \
			_timers.robot_attack < (iv+2))

   if(PHASE2){
      _timers.robot_attack++;
      return;
   }


#define PHASE3		(_timers.robot_attack >= (iv+2) && \
			_timers.robot_attack < (iv+8))

   if(PHASE3){

      tmpdir = j->p_dir;
      j->p_dir = predict_dir(olddir, j->p_dir, edist);
      get_torp_course(j, &tcrs, me->p_x, me->p_y);
      j->p_dir = tmpdir;
      if(DEBUG & DEBUG_WARFARE){
	 printf("phase II %d\n",_timers.robot_attack+(iv+2));
	 printf("edist: %d\n", edist);
      }
      req_torp(tcrs);
      _timers.robot_attack++;
      return;
   }
   else if(_timers.robot_attack >= (iv+8)){
      _timers.robot_attack = 0;
      return;
   }

#ifdef nodef
   else{
      _timers.pause_ra = (3*edist)/4000;
   }

#define PHASE4		(_timers.robot_attack >= (iv+8) && \
			_timers.robot_attack < (_timers.pause_ra+iv+8))

   if(PHASE3){
      /* pause between attacks */
      if(_timers.robot_attack == (_timers.pause_ra+iv+8)){
	 _timers.robot_attack = 0;
	 _timers.pause_ra = 0;
	 if(DEBUG & DEBUG_WARFARE){
	    printf("finished waiting.\n");
	 }
      }
      else
	 _timers.robot_attack ++;
      return;
   }
#endif

   _timers.robot_attack = 0;
   mprintf("unknown phase. %d\n", _timers.robot_attack);
}

ship_factor(s)

   int	s;
{
   switch(s){
      
      case GALAXY :		return 0;
      case BATTLESHIP : 	return 0;	/* tuned to battleship */
      case CRUISER :		return 3;	/* xx:same torpspeed */
      case DESTROYER : 		return 8;
      case ASSAULT:		return 10;
      case SCOUT:		return 10;

      default:	return 5;
   }
}
